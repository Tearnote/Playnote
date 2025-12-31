/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <system_error>
#include <exception>
#include <stdexcept>
#include <cerrno>
#include "quill/bundled/fmt/base.h"
#include "preamble/utility.hpp"
#include "preamble/string.hpp"

namespace playnote {

using std::exception;
using std::logic_error;
using std::runtime_error;
using std::current_exception;

// An arbitrary exception type with a formatted message
template<typename Err, typename... Args>
auto typed_error_fmt(fmtquill::format_string<Args...> fmt, Args&&... args) -> Err
{
	return Err{format(fmt, forward<Args>(args)...)};
}

// A std::runtime_error with a formatted message
template<typename... Args>
auto runtime_error_fmt(fmtquill::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::runtime_error>(fmt, forward<Args>(args)...);
};

// A std::logic_error with a formatted message
template<typename... Args>
auto logic_error_fmt(fmtquill::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::logic_error>(fmt, forward<Args>(args)...);
};

// A std::system_error wrapper with default error type and errno use
inline auto system_error(string const& msg)
{
	return std::system_error{errno, std::generic_category(), msg};
};

// A std::system_error with a formatted message, using errno automatically
template<typename... Args>
auto system_error_fmt(fmtquill::format_string<Args...> fmt, Args&&... args)
{
	return std::system_error{errno, std::generic_category(), format(fmt, forward<Args>(args)...)};
};

}
