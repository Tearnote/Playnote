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

	client->Release();
}

inline void cleanup() noexcept
{
	CoUninitialize();
}

}
