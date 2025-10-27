/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <filesystem>
#include <atomic>
#include <future>
#include <thread>
#include <mutex>
#include <latch>

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
	using std::filesystem::remove;
	using std::filesystem::rename;
}
using std::jthread;
using std::this_thread::sleep_for;
using std::this_thread::yield;
using std::atomic;
using std::mutex;
using std::lock_guard;
using std::latch;
using std::promise;
using std::future;
using std::future_status;

}
