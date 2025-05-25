/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

macros/assert.hpp:
Wrapper for assert macros of libassert. Registers the assert handler, ensures consistent behavior.
*/

#ifndef PLAYNOTE_MACROS_ASSERT_HPP
#define PLAYNOTE_MACROS_ASSERT_HPP

#include <stdexcept>
#include "libassert/assert.hpp"

// ASSERT() - checked in both debug and release
// ASSUME() - checked in debug only
// _VAL suffix - returns the result of the tested expression.
//     For comparisons, the left side is returned.

#endif
