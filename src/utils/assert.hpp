/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

assert.hpp:
Wrapper for assert macros of libassert. Registers the assert handler, ensures consistent behavior.
*/

#pragma once
#include <stdexcept>
#include "libassert/assert.hpp"

// ASSERT() - checked in both debug and release
// ASSUME() - checked in debug only
// PANIC() - unconditional failure in both debug and release
// _VAL suffix - returns the result of the tested expression.
//     For comparisons, the left side is returned.
