#ifndef PLAYNOTE_UTIL_CONCEPTS_H
#define PLAYNOTE_UTIL_CONCEPTS_H

#include <type_traits>

// Built-in type with defined arithmetic operations (+, -, *, /)
template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

#endif
