/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/wasapi.hpp:
WASAPI wrapper for Windows audio support.
*/

#pragma once

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <combaseapi.h>
#include <avrt.h>
#include "preamble.hpp"
#include "lib/pipewire.hpp"

namespace playnote::lib::wasapi {

using pw::Sample;

struct Context {
	uint32 sampling_rate;
	uint32 buffer_size;
	IAudioClient* client;
	IAudioRenderClient* renderer;
	jthread buffer_thread;
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
auto init(ProcessCallback on_process, T* userdata) -> Context
{
	ret_check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM");
	auto* enumerator = static_cast<IMMDeviceEnumerator*>(nullptr);
	ret_check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&enumerator)));
	auto* device = static_cast<IMMDevice*>(nullptr);
	ret_check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));
	enumerator->Release();
	auto* client = static_cast<IAudioClient3*>(nullptr);
	ret_check(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)));
	device->Release();

	auto* mix_format = static_cast<WAVEFORMATEX*>(nullptr);
	ret_check(client->GetMixFormat(&mix_format));
	auto f32 = WAVEFORMATEXTENSIBLE{
		.Format = WAVEFORMATEX{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = mix_format->nSamplesPerSec,
			.nAvgBytesPerSec = mix_format->nSamplesPerSec * 8,
			.nBlockAlign = 8,
			.wBitsPerSample = 32,
			.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
		},
		.Samples = {32},
		.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
		.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
	};
	CoTaskMemFree(mix_format);

	auto default_period = uint32{0};
	auto fundamental_period = uint32{0};
	auto min_period = uint32{0};
	auto max_period = uint32{0};
	ret_check(client->GetSharedModeEnginePeriod(reinterpret_cast<WAVEFORMATEX*>(&f32),
		&default_period, &fundamental_period, &min_period, &max_period));
	ret_check(client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		min_period, reinterpret_cast<WAVEFORMATEX*>(&f32), nullptr));
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

	return Context{
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
}

inline auto dequeue_buffer(Context& ctx) -> span<Sample>
{
	auto padding = uint32{0};
	ret_check(ctx.client->GetCurrentPadding(&padding));
	auto const actual_size = ctx.buffer_size - padding;
	auto* buffer = static_cast<Sample*>(nullptr);
	ret_check(ctx.renderer->GetBuffer(actual_size, reinterpret_cast<BYTE**>(&buffer)));
	return {buffer, actual_size};
}

inline void enqueue_buffer(Context& ctx, span<Sample> buffer)
{
	ret_check(ctx.renderer->ReleaseBuffer(buffer.size(), 0));
}

}
