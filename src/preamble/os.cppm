/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/os.cppm:
Imports of OS-specific functionality.
*/

module;
#include <filesystem>
#include <thread>
#include <latch>
#include "msd/channel.hpp"

export module playnote.preamble:os;

namespace playnote {

namespace fs {
	export using std::filesystem::path;
	export using std::filesystem::status;
	export using std::filesystem::exists;
	export using std::filesystem::is_regular_file;
	export using std::filesystem::recursive_directory_iterator;
	export using std::filesystem::relative;
}
export using std::jthread;
export using std::this_thread::sleep_for;
export using std::this_thread::yield;
export using std::atomic;
export using std::mutex;
export using std::lock_guard;
export using std::latch;
export using msd::channel;

}

// Helping ADL out here a bit
namespace std::filesystem {
export using std::filesystem::begin;
export using std::filesystem::end;
}
