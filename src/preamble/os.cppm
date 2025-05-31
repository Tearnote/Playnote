/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/os.cppm:
Imports of OS-specific functionality.
*/

module;
#include <filesystem>
#include <thread>

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

}

// Helping ADL out here a bit
namespace std::filesystem {
export using std::filesystem::begin;
export using std::filesystem::end;
}
