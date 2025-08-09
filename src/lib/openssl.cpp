/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/openssl.cpp:
Implementation file for lib/openssl.hpp.
*/

#include "lib/openssl.hpp"

#include <openssl/evp.h>
#include "preamble.hpp"

namespace playnote::lib::openssl {

auto md5(span<byte const> data) -> array<byte, 16>
{
	auto result = array<byte, 16>{};
	EVP_Q_digest(nullptr, "MD5", nullptr, data.data(), data.size(), reinterpret_cast<unsigned char*>(result.data()), nullptr);
	return result;
}

}
