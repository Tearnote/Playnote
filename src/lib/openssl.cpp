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

auto md5(span<byte const> data) -> MD5
{
	auto result = MD5{};
	EVP_Q_digest(nullptr, "MD5", nullptr, data.data(), data.size(), reinterpret_cast<unsigned char*>(result.data()), nullptr);
	return result;
}

auto md5_to_hex(const MD5& md5) -> string
{
	auto result = string{};
	result.reserve(md5.size() * 2);
	for (auto b: md5) {
		auto const hex = format("{:02x}", static_cast<uint8>(b));
		result.append(hex);
	}
	return result;
}

}
