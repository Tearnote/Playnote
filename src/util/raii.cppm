module;
#include <functional>
#include <optional>

export module playnote.util.raii;

import playnote.stx.callable;

namespace playnote::util {

// A wrapper for a C-style resource that guarantees running the cleanup function
export template<typename T, stx::callable<void(T&)> Func>
class RAIIResource {
public:
	RAIIResource() = default;
	RAIIResource(T const& resource): resource(resource) {}
	~RAIIResource() { if (resource) std::invoke(Func{}, *resource); }

	RAIIResource(RAIIResource const&) = delete;
	auto operator=(RAIIResource const&) -> RAIIResource& = delete;
	RAIIResource(RAIIResource&& other) noexcept { *this = std::move(other); }
	auto operator=(RAIIResource&&) noexcept -> RAIIResource&;

private:
	std::optional<T> resource{std::nullopt};
};

template<typename T, stx::callable<void(T&)> Func>
auto RAIIResource<T, Func>::operator=(RAIIResource&& other) noexcept -> RAIIResource&
{
	resource = std::move(other.resource);
	other.resource = std::nullopt;
	return *this;
}

}
