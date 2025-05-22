module;
#include <cstdint>
#include <cstddef>

export module playnote.preamble:types;

namespace playnote {

export using int8 = std::int8_t;
export using int16 = std::int16_t;
export using int32 = std::int32_t;
export using int64 = std::int64_t;
export using uint8 = std::uint8_t;
export using uint16 = std::uint16_t;
export using uint32 = std::uint32_t;
export using uint64 = std::uint64_t;
export using usize = std::size_t;
export using isize = std::ptrdiff_t;
export using std::byte;

}
