module;
#include <stdexcept>
#include <utility>
#include <format>

export module playnote.stx.except;

namespace playnote::stx {

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

}
