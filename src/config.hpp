/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.hpp:
Global game configuration - controls which features are on/off depending on build configuration,
and their hardcoded values.
*/

#pragma once
#include "preamble.hpp"
#include "logger.hpp"

namespace playnote {

inline constexpr auto AppTitle = "Playnote";
inline constexpr auto AppVersion = to_array({0u, 0u, 3u});

// Make sure BUILD_TYPE is specified
#define BUILD_DEBUG 0
#define BUILD_RELDEB 1
#define BUILD_RELEASE 2
#ifndef BUILD_TYPE
#error Build type not defined
#endif
#if (BUILD_TYPE != BUILD_DEBUG) && (BUILD_TYPE != BUILD_RELDEB) && (BUILD_TYPE != BUILD_RELEASE)
#error Build type incorrectly defined
#endif

enum class Build {
	Debug,
	RelDeb,
	Release,
};
#if BUILD_TYPE == BUILD_DEBUG
inline constexpr auto BuildType = Build::Debug;
#elif BUILD_TYPE == BUILD_RELDEB
inline constexpr auto BuildType = Build::RelDeb;
#else
inline constexpr auto BuildType = Build::Release;
#endif

// Detect target
#define TARGET_WINDOWS 0
#define TARGET_LINUX 1
#ifdef _WIN32
#define TARGET TARGET_WINDOWS
#elifdef __linux__
#define TARGET TARGET_LINUX
#else
#error Unknown target platform
#endif

enum class Target {
	Windows,
	Linux,
};
#if TARGET == TARGET_WINDOWS
inline constexpr auto BuildTarget = Target::Windows;
#elif TARGET == TARGET_LINUX
inline constexpr auto BuildTarget = Target::Linux;
#endif

// Whether to use assertions
#if BUILD_TYPE == BUILD_RELEASE
inline constexpr auto AssertionsEnabled = false;
#else
inline constexpr auto AssertionsEnabled = true;
#endif

// Whether to name threads for debugging
#if BUILD_TYPE != BUILD_RELEASE
inline constexpr auto ThreadNamesEnabled = true;
#else
inline constexpr auto ThreadNamesEnabled = false;
#endif

// Level of logging to file and/or console
#if BUILD_TYPE != BUILD_RELEASE
inline constexpr auto LogLevelGlobal = Logger::Level::TraceL1;
inline constexpr auto LogLevelGraphics = Logger::Level::Warning;
inline constexpr auto LogLevelBMSBuild = Logger::Level::Debug;
#else
inline constexpr auto LogLevelGlobal = Logger::Level::Info;
inline constexpr auto LogLevelGraphics = Logger::Level::Info;
inline constexpr auto LogLevelBMSBuild = Logger::Level::Info;
#endif

// Whether Vulkan validation layers are enabled
#if BUILD_TYPE == BUILD_DEBUG && TARGET == TARGET_LINUX
inline constexpr auto VulkanValidationEnabled = true;
#else
inline constexpr auto VulkanValidationEnabled = false;
#endif

// Logfile location
#if BUILD_TYPE == BUILD_DEBUG
inline constexpr auto LogfilePath = "playnote-debug.log"sv;
#elif BUILD_TYPE == BUILD_RELDEB
inline constexpr auto LogfilePath = "playnote-reldeb.log"sv;
#else
inline constexpr auto LogfilePath = "playnote.log"sv;
#endif

}
