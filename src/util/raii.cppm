/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/raii.cppm:
A wrapper for a C-style resource, guaranteeing that the cleanup function will be executed.
*/

module;

export module playnote.util.raii;

import playnote.preamble;

namespace playnote::util {

export template<typename T, callable<void(T&)> Func>
class RAIIResource {
public:
	using Inner = T;

	RAIIResource() = default;
	RAIIResource(T const& resource): resource(resource) {}
	~RAIIResource() { if (resource) invoke(Func{}, *resource); }

	auto get() -> T* { return &resource.value(); }
	auto get() const -> T const* { return &resource.value(); }
	auto operator*() -> T& { return *get(); }
	auto operator*() const -> T const& { return *get(); }
	auto operator->() -> T* { return get(); }
	auto operator->() const -> T const* { return get(); }

	// Safely moveable
	RAIIResource(RAIIResource const&) = delete;
	auto operator=(RAIIResource const&) -> RAIIResource& = delete;
	RAIIResource(RAIIResource&& other) noexcept { *this = move(other); }
	auto operator=(RAIIResource&&) noexcept -> RAIIResource&;

	operator bool() const { return resource; }

private:
	optional<T> resource{nullopt}; // nullopt represents uninitialized and moved-out-from states
};

template<typename T, callable<void(T&)> Func>
auto RAIIResource<T, Func>::operator=(RAIIResource&& other) noexcept -> RAIIResource&
{
	resource = move(other.resource);
	other.resource = nullopt;
	return *this;
}

}
