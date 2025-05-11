module;
#include <typeindex>
#include <utility>
#include <memory>
#include <tuple>
#include "libassert/assert.hpp"
#include "ankerl/unordered_dense.h"

export module playnote.util.locator;

namespace playnote::util {

// Holder of instances of arbitrary service classes
// Instances are type-erased on insertion, and accessed by their type via RTTI
// Inverse destruction order is maintained via RAII stubs
export class Locator {
	class Stub {
	public:
		Stub(Locator& parent, std::type_index slot): parent{parent}, slot{slot} {}
		~Stub() { parent.services.erase(slot); }

		// Moving would break destructor order
		Stub(Stub const& other) = delete;
		auto operator=(Stub const& other) -> Stub& = delete;
		Stub(Stub&& other) = delete;
		auto operator=(Stub&& other) -> Stub& = delete;

	private:
		Locator& parent;
		std::type_index slot;
	};

public:
	// Create and register a service, forwarding given arguments to the constructor
	// A stub is returned, which will destroy the service when the stub goes out of scope
	// The service must not exist yet
	template<typename T, typename... Args>
	auto provide(Args&&... args) -> std::pair<T&, Stub>;

	// Retrieve an instance of a previously registered service
	// The service must exist
	template<typename T>
	[[nodiscard]] auto get() -> T&;

	// Replace a previously registered service with a new instance,
	// without affecting its lifetime (the existing stub still applies)
	// The service must exist
	template<typename T, typename... Args>
	void replace(Args&&... args);

	// Check if a service exists
	template<typename T>
	[[nodiscard]] auto exists() const -> bool;

private:
	ankerl::unordered_dense::map<std::type_index, std::shared_ptr<void>> services{};
};

template<typename T, typename ... Args>
auto Locator::provide(Args&&... args) -> std::pair<T&, Stub> {
	auto slot = std::type_index{typeid(T)};
	ASSUME(!services.contains(slot));
	services.emplace(slot, std::make_shared<T>(std::forward<Args>(args)...));
	return {
		std::piecewise_construct, // Awkward but Stub needs to be constructed in-place
		std::make_tuple(std::ref(*std::static_pointer_cast<T>(services.at(slot)))),
		std::make_tuple(std::ref(*this), slot)
	};
}

template<typename T>
auto Locator::get() -> T& {
	auto slot = std::type_index{typeid(T)};
	ASSUME(services.contains(slot));
	return *std::static_pointer_cast<T>(services.at(slot));
}

template<typename T, typename ... Args>
void Locator::replace(Args&&... args) {
	auto slot = std::type_index{typeid(T)};
	ASSUME(services.contains(slot));
	services.at(slot) = std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T>
auto Locator::exists() const -> bool {
	auto slot = std::type_index{typeid(T)};
	return services.contains(slot);
}
}
