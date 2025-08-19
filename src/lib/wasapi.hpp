/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/wasapi.hpp:
WASAPI wrapper for Windows audio support.
*/

#pragma once

#define INITGUID
#include <guiddef.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <combaseapi.h>
#include <avrt.h>
#include "preamble.hpp"
#include "logger.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::wasapi {

struct Context {
	bool exclusive_mode;
	SampleFormat sample_format;
	uint32 sampling_rate;
	uint32 buffer_size;
	IAudioClient* client;
	IAudioRenderClient* renderer;
	jthread buffer_thread;
	std::byte* buffer;
	vector<Sample> client_buffer;
	shared_ptr<atomic<bool>> running_signal;
};

inline void ret_check(HRESULT hr, string_view message = "WASAPI error")
{
	if (FAILED(hr)) {
		CRIT("{}: {}", message, hr);
		throw system_error_fmt("{}: {}", message, hr);
	}
}

template<typename T>
static auto ptr_check(T* ptr) -> T*
{
	if (!ptr) throw runtime_error_fmt("WASAPI error: {}", GetLastError());
	return ptr;
}

template<typename T, typename U>
auto to_reference_time(duration<T, U> time) -> REFERENCE_TIME
{
	return duration_cast<nanoseconds>(time).count() / 100;
}

using ProcessCallback = void(*)(void*);

template<typename T>
auto init(bool exclusive_mode, ProcessCallback on_process, T* userdata) -> Context
{
	ret_check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM");
	auto* enumerator = static_cast<IMMDeviceEnumerator*>(nullptr);
	ret_check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&enumerator)));
	auto* device = static_cast<IMMDevice*>(nullptr);
	ret_check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));
	enumerator->Release();

	auto sample_format = SampleFormat{};
	if (!exclusive_mode) {
		sample_format = SampleFormat::Float32;
	} else {
		auto* properties = static_cast<IPropertyStore*>(nullptr);
		ret_check(device->OpenPropertyStore(STGM_READ, &properties));
		auto format_prop = PROPVARIANT{};
		PropVariantInit(&format_prop);
		ret_check(properties->GetValue(PKEY_AudioEngine_DeviceFormat, &format_prop));
		auto* exclusive_format = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format_prop.blob.pBlobData);
		if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && exclusive_format->Samples.wValidBitsPerSample == 32)
			sample_format = SampleFormat::Float32;
		else if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && exclusive_format->Samples.wValidBitsPerSample == 16)
			sample_format = SampleFormat::Int16;
		else if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && exclusive_format->Samples.wValidBitsPerSample == 24)
			sample_format = SampleFormat::Int24;
		else throw runtime_error{"Unknown WASAPI exclusive mode device-native sample format"};
	}

	auto* client = static_cast<IAudioClient3*>(nullptr);
	ret_check(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)));

	auto* mix_format = static_cast<WAVEFORMATEX*>(nullptr);
	ret_check(client->GetMixFormat(&mix_format));
	auto f32 = WAVEFORMATEXTENSIBLE{
		.Format = WAVEFORMATEX{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = mix_format->nSamplesPerSec,
			.nAvgBytesPerSec = mix_format->nSamplesPerSec * (32 / 8 * 2),
			.nBlockAlign = 8,
			.wBitsPerSample = 32,
			.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
		},
		.Samples = {32},
		.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
		.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
	};
	auto i16 = WAVEFORMATEXTENSIBLE{
		.Format = WAVEFORMATEX{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = mix_format->nSamplesPerSec,
			.nAvgBytesPerSec = mix_format->nSamplesPerSec * (16 / 8 * 2),
			.nBlockAlign = 8,
			.wBitsPerSample = 16,
			.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
		},
		.Samples = {16},
		.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
		.SubFormat = KSDATAFORMAT_SUBTYPE_PCM,
	};
	auto i24 = WAVEFORMATEXTENSIBLE{
		.Format = WAVEFORMATEX{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = mix_format->nSamplesPerSec,
			.nAvgBytesPerSec = mix_format->nSamplesPerSec * (32 / 8 * 2),
			.nBlockAlign = 8,
			.wBitsPerSample = 32,
			.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
		},
		.Samples = {24},
		.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
		.SubFormat = KSDATAFORMAT_SUBTYPE_PCM,
	};
	auto& format = [&]() -> WAVEFORMATEXTENSIBLE& {
		switch (sample_format) {
		case SampleFormat::Float32: return f32;
		case SampleFormat::Int16: return i16;
		case SampleFormat::Int24: return i24;
		default: throw logic_error{"Unknown WASAPI sample format"};
		}
	}();
	CoTaskMemFree(mix_format);

	if (!exclusive_mode) {
		auto default_period = uint32{0};
		auto fundamental_period = uint32{0};
		auto min_period = uint32{0};
		auto max_period = uint32{0};
		ret_check(client->GetSharedModeEnginePeriod(reinterpret_cast<WAVEFORMATEX*>(&format),
			&default_period, &fundamental_period, &min_period, &max_period));
		ret_check(client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			min_period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr));
	} else {
		auto default_period = REFERENCE_TIME{0};
		auto min_period = REFERENCE_TIME{0};
		ret_check(client->GetDevicePeriod(&default_period, &min_period));
		TRACE("default period: {}ns, min_period: {}ns", default_period * 100, min_period * 100);
		auto hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			min_period, min_period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr);
		if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			auto buffer_size = uint32{};
			ret_check(client->GetBufferSize(&buffer_size));
			auto period = static_cast<REFERENCE_TIME>((10000.0 * 1000 / format.Format.nSamplesPerSec * buffer_size) + 0.5);
			client->Release();
			ret_check(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)));
			ret_check(client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				period, period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr));
		} else {
			ret_check(hr);
		}
	}
	device->Release();
	auto buffer_event = ptr_check(CreateEvent(nullptr, false, false, nullptr));
	ret_check(client->SetEventHandle(buffer_event));
	auto buffer_size = uint32{0};
	ret_check(client->GetBufferSize(&buffer_size));
	DEBUG("WASAPI buffer size: {} samples", buffer_size);
	auto* renderer = static_cast<IAudioRenderClient*>(nullptr);
	ret_check(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&renderer)));
	auto running_signal = make_shared<atomic<bool>>(true);

	// Prefill first buffer
	auto* buffer = static_cast<Sample*>(nullptr);
	ret_check(renderer->GetBuffer(buffer_size, reinterpret_cast<BYTE**>(&buffer)));
	auto buffer_span = span{buffer, buffer_size};
	fill(buffer_span, Sample{0.0f, 0.0f});
	ret_check(renderer->ReleaseBuffer(buffer_size, 0));

	auto buffer_thread = jthread{[=]() {
		auto rtprio_taskid = 0ul;
		auto rtprio = ptr_check(AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &rtprio_taskid));
		ret_check(client->Start());

		while (running_signal->load()) {
			if (auto ret = WaitForSingleObject(buffer_event, 2000); ret != WAIT_OBJECT_0) {
				running_signal->store(false);
				break;
			}

			on_process(userdata);
		}

		client->Stop();
		AvRevertMmThreadCharacteristics(rtprio);
	}};

	INFO("WASAPI client initialized");
	return Context{
		.exclusive_mode = exclusive_mode,
		.sample_format = sample_format,
		.sampling_rate = f32.Format.nSamplesPerSec,
		.buffer_size = buffer_size,
		.client = client,
		.renderer = renderer,
		.buffer_thread = move(buffer_thread),
		.running_signal = move(running_signal),
	};
}

inline void cleanup(Context&& ctx) noexcept
{
	ctx.running_signal->store(false);
	ctx.buffer_thread.join();
	ctx.renderer->Release();
	ctx.client->Release();
	CoUninitialize();
	INFO("WASAPI client cleaned up");
}

inline auto dequeue_buffer(Context& ctx) -> span<Sample>
{
	auto padding = uint32{0};
	auto actual_size = uint32{0};
	if (!ctx.exclusive_mode) {
		ret_check(ctx.client->GetCurrentPadding(&padding));
		actual_size = ctx.buffer_size - padding;
	} else {
		actual_size = ctx.buffer_size;
	}
	ret_check(ctx.renderer->GetBuffer(actual_size, reinterpret_cast<BYTE**>(&ctx.buffer)));
	ctx.client_buffer.resize(actual_size);
	fill(ctx.client_buffer, Sample{});
	return ctx.client_buffer;
}

inline void enqueue_buffer(Context& ctx)
{
	struct Int16Sample {
		int16 left;
		int16 right;
	};
	struct Int24Sample {
		int32 left;
		int32 right;
	};
	switch (ctx.sample_format) {
	case SampleFormat::Float32:
		copy(ctx.client_buffer, reinterpret_cast<Sample*>(ctx.buffer));
		break;
	case SampleFormat::Int16:
		transform(ctx.client_buffer, reinterpret_cast<Int16Sample*>(ctx.buffer), [](auto const& sample) {
			return Int16Sample{
				.left = static_cast<int16>(lround(sample.left * 0x7FFF) & 0xFFFF),
				.right = static_cast<int16>(lround(sample.right * 0x7FFF) & 0xFFFF),
			};
		});
		break;
	case SampleFormat::Int24:
		transform(ctx.client_buffer, reinterpret_cast<Int24Sample*>(ctx.buffer), [](auto const& sample) {
			return Int24Sample{
				.left = (lround(sample.left * 0x7FFFFF) & 0xFFFFFF) << 8,
				.right = (lround(sample.right * 0x7FFFFF) & 0xFFFFFF) << 8,
			};
		});
		break;
	default: throw logic_error{"Unknown WASAPI sample format"};
	}
	ret_check(ctx.renderer->ReleaseBuffer(ctx.client_buffer.size(), 0));
}

}
