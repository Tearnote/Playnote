/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/openssl.cppm:
Wrapper for mio file mapping.
*/

module;
#include <openssl/evp.h>
#include "preamble.hpp"

export module playnote.lib.openssl;

namespace playnote::lib::openssl {

// Calculate and return the MD5 hash of provided data.
export auto md5(span<byte const> data) -> array<byte, 16>
{
	auto result = array<byte, 16>{};
	EVP_Q_digest(nullptr, "MD5", nullptr, data.data(), data.size(), reinterpret_cast<unsigned char*>(result.data()), nullptr);
	return result;
}

}
