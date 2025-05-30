/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.cppm:
Global game configuration - controls which features are on/off depending on build configuration,
and their hardcoded values.
*/

export module playnote.config;

import playnote.preamble;
import playnote.logger;

namespace playnote {

export inline constexpr auto AppTitle = "Playnote";
export inline constexpr auto AppVersion = to_array({0u, 0u, 0u});

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

export enum class Build {
	Debug,
	RelDeb,
	Release,
};
#if BUILD_TYPE == BUILD_DEBUG
export inline constexpr auto BuildType = Build::Debug;
#elif BUILD_TYPE == BUILD_RELDEB
export inline constexpr auto BuildType = Build::RelDeb;
#else
export inline constexpr auto BuildType = Build::Release;
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

export enum class Target {
	Windows,
	Linux,
};
#if TARGET == TARGET_WINDOWS
export inline constexpr auto BuildTarget = Target::Windows;
#elif TARGET == TARGET_LINUX
export inline constexpr auto BuildTarget = Target::Linux;
#endif

// Whether to use assertions
#if BUILD_TYPE == BUILD_RELEASE
export inline constexpr auto AssertionsEnabled = false;
#else
export inline constexpr auto AssertionsEnabled = true;
#endif

// Whether to name threads for debugging
#if BUILD_TYPE != BUILD_RELEASE
export inline constexpr auto ThreadNamesEnabled = true;
#else
export inline constexpr auto ThreadNamesEnabled = false;
#endif

// Level of logging to file and/or console
#if BUILD_TYPE != BUILD_RELEASE
export inline constexpr auto LogLevelGlobal = Logger::Level::TraceL1;
export inline constexpr auto LogLevelGraphics = Logger::Level::Warning;
export inline constexpr auto LogLevelBMSBuild = Logger::Level::Debug;
#else
export inline constexpr auto LogLevelGlobal = util::Logger::Info;
export inline constexpr auto LogLevelGraphics = util::Logger::Info;
export inline constexpr auto LogLevelBMSBuild = util::Logger::Info;
#endif

// Whether Vulkan validation layers are enabled
#if BUILD_TYPE == BUILD_DEBUG
export inline constexpr auto VulkanValidationEnabled = true;
#else
export inline constexpr auto VulkanValidationEnabled = false;
#endif

// Logfile location
#if BUILD_TYPE == BUILD_DEBUG
export inline constexpr auto LogfilePath = "playnote-debug.log"sv;
#elif BUILD_TYPE == BUILD_RELDEB
export inline constexpr auto LogfilePath = "playnote-reldeb.log"sv;
#else
export inline constexpr auto LogfilePath = "playnote.log"sv;
#endif

}
