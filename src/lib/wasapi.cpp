/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/wasapi.cpp:
Implementation file for lib/wasapi.hpp.
*/

#include "lib/wasapi.hpp"

#define INITGUID
#include <guiddef.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <combaseapi.h>
#include <avrt.h>
#include "preamble.hpp"
#include "logger.hpp"

namespace playnote::lib::wasapi {

// Helper functions for error handling

static void ret_check(HRESULT hr, string_view message = "WASAPI error")
{
	if (FAILED(hr)) {
		CRIT("{}: {}", message, hr);
		throw runtime_error_fmt("{}: {}", message, hr);
	}
}

template<typename T>
static auto ptr_check(T* ptr) -> T*
{
	if (!ptr) throw runtime_error_fmt("WASAPI error: {}", GetLastError());
	return ptr;
}

// Any duration -> REFERENCE_TIME
template<typename T, typename U>
static auto to_reference_time(duration<T, U> time) -> REFERENCE_TIME
{
	return duration_cast<nanoseconds>(time).count() / 100;
}

// Function executed in the realtime audio thread.
// Raises its own priority, begins sample processing, and calls the user function every time
// a buffer is signalled to be ready.
static void buffer_thread(Context_t* ctx, HANDLE buffer_event)
{
	auto rtprio_taskid = 0ul;
	auto rtprio = ptr_check(AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &rtprio_taskid));
	ret_check(ctx->client->Start());
	auto client_buffer = vector<Sample>{};

	while (ctx->running_signal->load()) {
		if (auto ret = WaitForSingleObject(buffer_event, 2000); ret != WAIT_OBJECT_0) {
			ctx->running_signal->store(false);
			break;
		}

		auto padding = uint32{0};
		auto actual_size = uint32{0};
		if (!ctx->exclusive_mode) {
			ret_check(ctx->client->GetCurrentPadding(&padding));
			actual_size = ctx->properties.buffer_size - padding;
		} else {
			actual_size = ctx->properties.buffer_size;
		}
		auto* buffer = static_cast<byte*>(nullptr);
		ret_check(ctx->renderer->GetBuffer(actual_size, reinterpret_cast<BYTE**>(&buffer)));
		client_buffer.resize(actual_size);
		fill(client_buffer, Sample{});

		ctx->processor(client_buffer);

		struct Int16Sample {
			int16 left;
			int16 right;
		};
		struct Int24Sample {
			int32 left;
			int32 right;
		};
		switch (ctx->properties.sample_format) {
		case SampleFormat::Float32:
			copy(client_buffer, reinterpret_cast<Sample*>(buffer));
			break;
		case SampleFormat::Int16:
			transform(client_buffer, reinterpret_cast<Int16Sample*>(buffer), [](auto const& sample) {
				return Int16Sample{
					.left = static_cast<int16>(lround(sample.left * 0x7FFF) & 0xFFFF),
					.right = static_cast<int16>(lround(sample.right * 0x7FFF) & 0xFFFF),
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
		ret_check(ctx->renderer->ReleaseBuffer(client_buffer.size(), 0));
	}

	ctx->client->Stop();
	AvRevertMmThreadCharacteristics(rtprio);
}

auto init(bool exclusive_mode, function<void(span<Sample>)>&& processor) -> Context
{
	auto ctx = make_unique<Context_t>();

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
	auto* renderer = static_cast<IAudioRenderClient*>(nullptr);
	ret_check(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&renderer)));
	auto running_signal = make_shared<atomic<bool>>(true);

	// Prefill first buffer
	auto* buffer = static_cast<Sample*>(nullptr);
	ret_check(renderer->GetBuffer(buffer_size, reinterpret_cast<BYTE**>(&buffer)));
	auto buffer_span = span{buffer, buffer_size};
	fill(buffer_span, Sample{0.0f, 0.0f});
	ret_check(renderer->ReleaseBuffer(buffer_size, 0));

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

	INFO("WASAPI client initialized");
	return ctx;
}

void cleanup(Context&& ctx) noexcept
{
	ctx->running_signal->store(false);
	ctx->buffer_thread.join();
	ctx->renderer->Release();
	ctx->client->Release();
	CoUninitialize();
	INFO("WASAPI client cleaned up");
}

}
