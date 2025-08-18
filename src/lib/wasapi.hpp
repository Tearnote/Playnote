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

namespace playnote::lib::wasapi {

struct Context {
	uint32 sampling_rate;
	IAudioClient* client;
	jthread buffer_thread;
	shared_ptr<atomic<bool>> running_signal;
};

inline void ret_check(HRESULT hr, string_view message = "WASAPI error")
{
	if (FAILED(hr)) throw system_error_fmt("{}: {}", message, hr);
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

inline auto init() -> Context
{
	ret_check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Failed to initialize COM");
	auto* enumerator = static_cast<IMMDeviceEnumerator*>(nullptr);
	ret_check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&enumerator)));
	auto device = static_cast<IMMDevice*>(nullptr);
	ret_check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));
	enumerator->Release();
	auto client = static_cast<IAudioClient*>(nullptr);
	ret_check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)));
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
	ret_check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		to_reference_time(5ms), 0, reinterpret_cast<WAVEFORMATEX*>(&f32), nullptr));
	auto buffer_event = ptr_check(CreateEvent(nullptr, false, false, nullptr));
	ret_check(client->SetEventHandle(buffer_event));
	auto running_signal = make_shared<atomic<bool>>(true);

	auto buffer_thread = jthread{[=]() {
		auto rtprio_taskid = 0ul;
		auto rtprio = ptr_check(AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &rtprio_taskid));
		ret_check(client->Start());

		while (running_signal->load()) {
			;
		}

		client->Stop();
		AvRevertMmThreadCharacteristics(rtprio);
	}};

	return Context{
		.sampling_rate = f32.Format.nSamplesPerSec,
		.client = client,
		.buffer_thread = move(buffer_thread),
		.running_signal = move(running_signal),
	};
}

inline void cleanup(Context&& ctx) noexcept
{
	ctx.running_signal->store(false);
	ctx.buffer_thread.join();
	ctx.client->Release();
	CoUninitialize();
}

}
