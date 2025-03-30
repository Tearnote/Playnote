#pragma once

#include <stdexcept>
#include <utility>
#include <format>

template<typename Err, typename... Args>
auto typed_error_fmt(std::format_string<Args...> fmt, Args&&... args) -> Err
{
	return Err(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
auto runtime_error_fmt(std::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::runtime_error>(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
auto logic_error_fmt(std::format_string<Args...> fmt, Args&&... args)
{
	return typed_error_fmt<std::logic_error>(fmt, std::forward<Args>(args)...);
};
