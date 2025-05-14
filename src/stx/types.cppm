/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

stx/types.cppm:
Explicitly sized shorthands for standard types.
*/

module;
#include <cstdint>
#include <cstddef>

export module playnote.stx.types;

namespace playnote::stx {

export using uint8 = std::uint8_t;
export using uint16 = std::uint16_t;
export using uint = std::uint32_t;
export using uint64 = std::uint64_t;
export using int8 = std::int8_t;
export using int16 = std::int16_t;
// int32 = int
export using int64 = std::int64_t;
export using usize = std::size_t;
export using isize = std::ptrdiff_t;

// Sanity checks
static_assert(sizeof(uint8) == 1);
static_assert(sizeof(uint16) == 2);
static_assert(sizeof(uint) == 4);
static_assert(sizeof(uint64) == 8);
static_assert(sizeof(int8) == 1);
static_assert(sizeof(int16) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(int64) == 8);

}
