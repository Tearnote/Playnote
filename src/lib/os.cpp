/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/os.hpp"

#include "utils/config.hpp"
#ifdef TARGET_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <timeapi.h>
#include <shellapi.h>
#include <dwrite.h>
#include <wrl/client.h>
#elifdef TARGET_LINUX
#include <fontconfig/fontconfig.h>
#include <linux/ioprio.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#endif
#include <cstdlib>
#include "mimalloc.h"
#include "preamble.hpp"
#include "utils/logger.hpp"

namespace playnote::lib::os {

#ifdef TARGET_WINDOWS
template<typename T>
using ComPtr = unique_resource<T*, decltype([](auto* p) { if (p) p->Release(); })>;

static void ret_check(HRESULT hr, string_view message = "DirectWrite error")
{ if (FAILED(hr)) throw runtime_error_fmt("{}: {:#x}", message, hr); }
#endif

void check_mimalloc()
{
	if (mi_version() <= 0) {
		WARN("mimalloc is not loaded");
		return;
	}
	INFO("mimalloc {}.{}.{} loaded", mi_version() / 100, mi_version() % 100 / 10, mi_version() % 10);
	auto* p = new int{};
	auto const new_ok = mi_is_in_heap_region(p);
	void* q = std::malloc(4);
	auto const malloc_ok = mi_is_in_heap_region(q);
	if (!new_ok || !malloc_ok)
		WARN("mimalloc override is not active (new: {}, malloc: {})", new_ok, malloc_ok);
	free(q);
	delete p;
}

void name_current_thread(string_view name)
{
#ifdef TARGET_WINDOWS
	auto const lname = std::wstring{name.begin(), name.end()}; // No reencoding; not expecting non-ASCII here
	auto const err = SetThreadDescription(GetCurrentThread(), lname.c_str());
	if (FAILED(err))
		throw runtime_error_fmt("Failed to set thread name: error {}", err);
#elifdef TARGET_LINUX
	auto const err = pthread_setname_np(pthread_self(), string{name}.c_str());
	if (err != 0)
		throw system_error("Failed to set thread name");
#endif
}

void lower_current_thread_priority()
{
#ifdef TARGET_WINDOWS
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#elifdef TARGET_LINUX
	auto param = sched_param{};
	if (pthread_setschedparam(pthread_self(), SCHED_IDLE, &param) != 0 ||
		syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0)) != 0)
		throw system_error("Failed to lower thread priority");
#endif
}

void begin_scheduler_period([[maybe_unused]] milliseconds period)
{
#ifdef TARGET_WINDOWS
	if (timeBeginPeriod(period.count()) != TIMERR_NOERROR)
		throw runtime_error{"Failed to initialize thread scheduler period"};
#endif
}

void end_scheduler_period([[maybe_unused]] milliseconds period) noexcept
{
#ifdef TARGET_WINDOWS
	timeEndPeriod(period.count());
#endif
}

void block_with_message([[maybe_unused]] string_view message)
{
#ifdef TARGET_WINDOWS
	MessageBoxA(nullptr, string{message}.c_str(), AppTitle, MB_OK);
#endif
}

auto get_subpixel_layout() -> SubpixelLayout
{
#ifdef TARGET_WINDOWS
	auto* factory_raw = static_cast<IDWriteFactory*>(nullptr);
	ret_check(DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&factory_raw)
	));
	auto factory = ComPtr<IDWriteFactory>{factory_raw};

	auto* params_raw = static_cast<IDWriteRenderingParams*>(nullptr);
	ret_check(factory->CreateRenderingParams(&params_raw));
	auto params = ComPtr<IDWriteRenderingParams>{params_raw};

	switch (params->GetPixelGeometry()) {
	case DWRITE_PIXEL_GEOMETRY_FLAT: return SubpixelLayout::None;
	case DWRITE_PIXEL_GEOMETRY_RGB:  return SubpixelLayout::HorizontalRGB;
	case DWRITE_PIXEL_GEOMETRY_BGR:  return SubpixelLayout::HorizontalBGR;
	default:                         return SubpixelLayout::Unknown;
	}

#elifdef TARGET_LINUX
	using Pattern = unique_resource<FcPattern*, decltype([](auto* p) { FcPatternDestroy(p); })>;

	auto pattern = Pattern{FcPatternCreate()};
	if (!pattern) throw runtime_error{"Failed to create Fontconfig pattern"};
	FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
	FcDefaultSubstitute(pattern.get());

	auto rgba = FC_RGBA_UNKNOWN;
	if (FcPatternGetInteger(pattern.get(), FC_RGBA, 0, &rgba) != FcResultMatch)
		return SubpixelLayout::Unknown;
	switch (rgba) {
		case FC_RGBA_RGB:  return SubpixelLayout::HorizontalRGB;
		case FC_RGBA_BGR:  return SubpixelLayout::HorizontalBGR;
		case FC_RGBA_VRGB: return SubpixelLayout::VerticalRGB;
		case FC_RGBA_VBGR: return SubpixelLayout::VerticalBGR;
		case FC_RGBA_NONE: return SubpixelLayout::None;
		default: return SubpixelLayout::Unknown;
	}
#endif
}

}
