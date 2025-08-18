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
#include "preamble.hpp"

namespace playnote::lib::wasapi {

inline void ret_check(HRESULT hr, string_view message = "WASAPI error")
{
	if (FAILED(hr)) throw system_error_fmt("{}: {}", message, hr);
}

template<typename T, typename U>
auto to_reference_time(duration<T, U> time) -> REFERENCE_TIME
{
	return duration_cast<nanoseconds>(time).count() / 100;
}

inline void init()
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

	client->Release();
}

inline void cleanup() noexcept
{
	CoUninitialize();
}

}
