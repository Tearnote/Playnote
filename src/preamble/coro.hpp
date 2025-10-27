/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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
