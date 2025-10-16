/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/coro.hpp:
Imports and helpers for C++20 coroutines.
*/

#pragma once
#include "coro/coro.hpp"

namespace playnote {

template<typename T = void>
using task = coro::task<T>;
using task_container = coro::task_container<coro::thread_pool>;
using coro::when_all;
using coro::when_any;
using coro::sync_wait;
using coro::thread_pool;
using coro_mutex = coro::mutex;

}
