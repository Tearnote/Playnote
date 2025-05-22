/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for container types.
*/

module;
#include <vector>
#include <array>
#include "ankerl/unordered_dense.h"

export module playnote.preamble:container;

namespace playnote {

export using std::vector;
export using std::array;
export template<typename Key, typename T, typename Hash = ankerl::unordered_dense::hash<Key>>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash>;

}
