/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/wasapi.hpp"

#define INITGUID
#include <guiddef.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <combaseapi.h>
#include <avrt.h>
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::wasapi {

// Helper functions for error handling

static void ret_check(HRESULT hr, string_view message = "WASAPI error")
{
	if (FAILED(hr)) throw runtime_error_fmt("{}: {:#x}", message, hr);
}

template<typename T>
static auto ptr_check(T* ptr) -> T*
{
	if (!ptr) throw runtime_error_fmt("WASAPI error: {:#x}", GetLastError());
	return ptr;
}

// Any duration -> REFERENCE_TIME
template<typename T, typename U>
static auto to_reference_time(duration<T, U> time) -> REFERENCE_TIME
{
	return duration_cast<nanoseconds>(time).count() / 100;
}

struct Int16Sample {
	int16_t left;
	int16_t right;
};

struct Int24Sample {
	int left;
	int right;
};

// Function executed in the realtime audio thread.
// Raises its own priority, begins sample processing, and calls the user function every time
// a buffer is signalled to be ready.
static void buffer_thread(Context_t* ctx, HANDLE buffer_event)
{
	auto rtprio_taskid = 0ul;
	auto rtprio = ptr_check(AvSetMmThreadCharacteristicsA("Pro Audio", &rtprio_taskid));
	ret_check(ctx->client->Start(), "Failed to start WASAPI stream");
	auto client_buffer = vector<Sample>{};

	while (ctx->running_signal->load()) {
		if (auto ret = WaitForSingleObject(buffer_event, 2000); ret != WAIT_OBJECT_0) {
			ctx->running_signal->store(false);
			break;
		}

		auto padding = uint{0};
		auto actual_size = uint{0};
		if (!ctx->exclusive_mode) {
			ret_check(ctx->client->GetCurrentPadding(&padding), "Failed to retrieve buffer padding");
			actual_size = ctx->properties.buffer_size - padding;
		} else {
			actual_size = ctx->properties.buffer_size;
		}
		auto* buffer = static_cast<byte*>(nullptr);
		ret_check(ctx->renderer->GetBuffer(actual_size, reinterpret_cast<BYTE**>(&buffer)), "Failed to retrieve WASAPI buffer");
		client_buffer.resize(actual_size);
		fill(client_buffer, Sample{});

		ctx->processor(client_buffer);

		switch (ctx->properties.sample_format) {
		case SampleFormat::Float32:
			copy(client_buffer, reinterpret_cast<Sample*>(buffer));
			break;
		case SampleFormat::Int16:
			transform(client_buffer, reinterpret_cast<Int16Sample*>(buffer), [](auto const& sample) {
				return Int16Sample{
					.left = static_cast<int16_t>(lround(sample.left * 0x7FFF)),
					.right = static_cast<int16_t>(lround(sample.right * 0x7FFF)),
				};
			});
			break;
		case SampleFormat::Int24:
			transform(client_buffer, reinterpret_cast<Int24Sample*>(buffer), [](auto const& sample) {
				return Int24Sample{
					.left = (lround(sample.left * 0x7FFFFF) & 0xFFFFFF) << 8,
					.right = (lround(sample.right * 0x7FFFFF) & 0xFFFFFF) << 8,
				};
			});
			break;
		default: throw logic_error{"Unknown WASAPI sample format"};
		}
		ret_check(ctx->renderer->ReleaseBuffer(client_buffer.size(), 0), "Failed to release WASAPI buffer");
	}

	ctx->client->Stop();
	AvRevertMmThreadCharacteristics(rtprio);
}

auto init(Logger::Category cat, bool exclusive_mode, function<void(span<Sample>)>&& processor, optional<nanoseconds> latency) -> Context
{
	auto ctx = make_unique<Context_t>();

	ret_check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM");
	auto* enumerator = static_cast<IMMDeviceEnumerator*>(nullptr);
	ret_check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&enumerator)), "Failed to retrieve IMMDevice interface");
	auto* device = static_cast<IMMDevice*>(nullptr);
	ret_check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device), "Failed to retrieve default audio device");
	enumerator->Release();

	auto* client = static_cast<IAudioClient3*>(nullptr);
	ret_check(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)), "Failed to retrieve IAudioClient3 interface");

	auto sample_format = SampleFormat{};
	auto sample_rate = 0u;
	if (!exclusive_mode) {
		sample_format = SampleFormat::Float32;
		auto* mix_format = static_cast<WAVEFORMATEX*>(nullptr);
		ret_check(client->GetMixFormat(&mix_format), "Failed to retrieve shared mixer format");
		sample_rate = mix_format->nSamplesPerSec;
		CoTaskMemFree(mix_format);
	} else {
		auto* properties = static_cast<IPropertyStore*>(nullptr);
		ret_check(device->OpenPropertyStore(STGM_READ, &properties), "Failed to open property store");
		auto format_prop = PROPVARIANT{};
		PropVariantInit(&format_prop);
		ret_check(properties->GetValue(PKEY_AudioEngine_DeviceFormat, &format_prop), "Failed to retrieve audio device preferred format");
		auto* exclusive_format = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format_prop.blob.pBlobData);
		if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && exclusive_format->Samples.wValidBitsPerSample == 32)
			sample_format = SampleFormat::Float32;
		else if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && exclusive_format->Samples.wValidBitsPerSample == 16)
			sample_format = SampleFormat::Int16;
		else if (exclusive_format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && exclusive_format->Samples.wValidBitsPerSample == 24)
			sample_format = SampleFormat::Int24;
		else throw runtime_error{"Unknown WASAPI exclusive mode device-native sample format"};
		sample_rate = exclusive_format->Format.nSamplesPerSec;
	}

	auto f32 = WAVEFORMATEXTENSIBLE{
		.Format = WAVEFORMATEX{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = sample_rate * (32 / 8 * 2),
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
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = sample_rate * (16 / 8 * 2),
			.nBlockAlign = 4,
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
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = sample_rate * (32 / 8 * 2),
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

	if (!exclusive_mode) {
		auto default_period = uint{0};
		auto fundamental_period = uint{0};
		auto min_period = uint{0};
		auto max_period = uint{0};
		ret_check(client->GetSharedModeEnginePeriod(reinterpret_cast<WAVEFORMATEX*>(&format),
			&default_period, &fundamental_period, &min_period, &max_period), "Failed to retrieve shared mode engine period");
		auto period = min_period;
		if (latency) {
			auto latency_frames = static_cast<double>(latency->count()) / 1'000'000'000.0 * sample_rate;
			if (latency_frames < min_period) WARN_AS(cat, "Could not set WASAPI latency below the minimum value of {}ms", static_cast<double>(min_period) / sample_rate * 1000.0);
			else if (latency_frames > max_period) WARN_AS(cat, "Could not set WASAPI latency above the maximum value of {}ms", static_cast<double>(max_period) / sample_rate * 1000.0);
			else period = latency_frames;
		}
		ret_check(client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr), "Failed to initialize WASAPI audio stream");
	} else {
		auto default_period = REFERENCE_TIME{0};
		auto min_period = REFERENCE_TIME{0};
		ret_check(client->GetDevicePeriod(&default_period, &min_period), "Failed to retrieve audio device period");
		auto period = min_period;
		if (latency) {
			auto latency_rt = to_reference_time(*latency);
			if (latency_rt < min_period) WARN_AS(cat, "Could not set WASAPI latency below the minimum value of {}ms", min_period / 10000);
			else period = latency_rt;
		}
		auto hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			period, period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr);
		if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			auto buffer_size = uint{};
			ret_check(client->GetBufferSize(&buffer_size), "Failed to retrieve audio buffer size");
			auto new_period = static_cast<REFERENCE_TIME>((10000.0 * 1000 / format.Format.nSamplesPerSec * buffer_size) + 0.5);
			client->Release();
			ret_check(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)), "Failed to re-retrieve IAudioClient3 interface");
			ret_check(client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				new_period, new_period, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr), "Failed to re-initialize WASAPI audio stream");
		} else {
			ret_check(hr, "Failed to initialize WASAPI audio stream");
		}
	}
	device->Release();
	auto buffer_event = ptr_check(CreateEvent(nullptr, false, false, nullptr));
	ret_check(client->SetEventHandle(buffer_event), "Failed to set WASAPI buffer callback");
	auto buffer_size = 0u;
	ret_check(client->GetBufferSize(&buffer_size), "Failed to retrieve audio buffer size");
	auto* renderer = static_cast<IAudioRenderClient*>(nullptr);
	ret_check(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&renderer)), "Failed to retrieve IAudioRenderClient interface");
	auto running_signal = make_shared<atomic<bool>>(true);

	// Prefill first buffer
	if (sample_format == SampleFormat::Int16) {
		auto* buffer = static_cast<Int16Sample*>(nullptr);
		ret_check(renderer->GetBuffer(buffer_size, reinterpret_cast<BYTE**>(&buffer)), "Failed to retrieve WASAPI buffer");
		auto buffer_span = span{buffer, buffer_size};
		fill(buffer_span, Int16Sample{0, 0});
		ret_check(renderer->ReleaseBuffer(buffer_size, 0), "Failed to release WASAPI buffer");
	} else if (sample_format == SampleFormat::Int24) {
		auto* buffer = static_cast<Int24Sample*>(nullptr);
		ret_check(renderer->GetBuffer(buffer_size, reinterpret_cast<BYTE**>(&buffer)), "Failed to retrieve WASAPI buffer");
		auto buffer_span = span{buffer, buffer_size};
		fill(buffer_span, Int24Sample{0, 0});
		ret_check(renderer->ReleaseBuffer(buffer_size, 0), "Failed to release WASAPI buffer");
	} else {
		auto* buffer = static_cast<Sample*>(nullptr);
		ret_check(renderer->GetBuffer(buffer_size, reinterpret_cast<BYTE**>(&buffer)), "Failed to retrieve WASAPI buffer");
		auto buffer_span = span{buffer, buffer_size};
		fill(buffer_span, Sample{0.0f, 0.0f});
		ret_check(renderer->ReleaseBuffer(buffer_size, 0), "Failed to release WASAPI buffer");
	}

	*ctx = Context_t{
		.properties = AudioProperties{
			.sampling_rate = f32.Format.nSamplesPerSec,
			.sample_format = sample_format,
			.buffer_size = buffer_size,
		},
		.exclusive_mode = exclusive_mode,
		.client = client,
		.renderer = renderer,
		.running_signal = move(running_signal),
		.processor = move(processor),
	};
	auto* ctx_addr = ctx.get();
	ctx->buffer_thread = jthread{[=]() { buffer_thread(ctx_addr, buffer_event); }};

	return ctx;
}

void cleanup(Context&& ctx) noexcept
{
	ctx->running_signal->store(false);
	ctx->buffer_thread.join();
	ctx->renderer->Release();
	ctx->client->Release();
	CoUninitialize();
}

}
