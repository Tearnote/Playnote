/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/os.hpp:
Imports of OS-specific functionality.
*/

#pragma once
#include <filesystem>
#include <thread>
#include <latch>
#include "msd/channel.hpp"

namespace playnote {

namespace fs {
	using std::filesystem::path;
	using std::filesystem::status;
	using std::filesystem::exists;
	using std::filesystem::is_regular_file;
	using std::filesystem::is_directory;
	using std::filesystem::create_directory;
	using std::filesystem::directory_iterator;
	using std::filesystem::recursive_directory_iterator;
	using std::filesystem::directory_entry;
	using std::filesystem::relative;
}
using std::jthread;
using std::this_thread::sleep_for;
using std::this_thread::yield;
using std::atomic;
using std::mutex;
using std::lock_guard;
using std::latch;
using msd::channel;

}
