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
	using Inner = T;

	RAIIResource() = default;
	RAIIResource(T const& resource): resource(resource) {}
	~RAIIResource() { if (resource) std::invoke(Func{}, *resource); }

	auto get() -> T& { return resource.value(); }
	auto get() const -> T const& { return resource.value(); }
	auto operator*() -> T& { return get(); }
	auto operator*() const -> T const& { return get(); }
	auto operator->() -> T* { return &get(); }
	auto operator->() const -> T const* { return &get(); }

	// Safely moveable
	RAIIResource(RAIIResource const&) = delete;
	auto operator=(RAIIResource const&) -> RAIIResource& = delete;
	RAIIResource(RAIIResource&& other) noexcept { *this = std::move(other); }
	auto operator=(RAIIResource&&) noexcept -> RAIIResource&;

	operator bool() const { return resource; }

private:
	std::optional<T> resource{std::nullopt}; // nullopt represents uninitialized and moved-out-from states
};

template<typename T, stx::callable<void(T&)> Func>
auto RAIIResource<T, Func>::operator=(RAIIResource&& other) noexcept -> RAIIResource&
{
	resource = std::move(other.resource);
	other.resource = std::nullopt;
	return *this;
}

}
