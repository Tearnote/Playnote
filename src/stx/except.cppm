/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

stx/except.cppm:
Additional exception types, adding formatting and convenience wrappers.
*/

module;
#include <system_error>
#include <stdexcept>
#include <utility>
#include <cerrno>
#include <format>

export module playnote.stx.except;

namespace playnote::stx {

// An arbitrary exception type with a formatted message
template<typename Err, typename... Args>
auto typed_error_fmt(std::format_string<Args...> fmt, Args&&... args) -> Err
{
	return Err{std::format(fmt, std::forward<Args>(args)...)};
}

// A std::runtime_error with a formatted message
export template<typename... Args>
auto runtime_error_fmt(std::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::runtime_error>(fmt, std::forward<Args>(args)...);
};

// A std::logic_error with a formatted message
export template<typename... Args>
auto logic_error_fmt(std::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::logic_error>(fmt, std::forward<Args>(args)...);
};

// A std::system_error with a formatted message, using errno automatically
export template<typename... Args>
auto system_error_fmt(std::format_string<Args...> fmt, Args&&... args)
{
	return std::system_error{errno, std::generic_category(), std::format(fmt, std::forward<Args>(args)...)};
};

}
