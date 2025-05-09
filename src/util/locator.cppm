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
export class Locator {
	class Stub {
	public:
		Stub(Locator& parent, std::type_index slot): parent{parent}, slot{slot} {}
		~Stub() { parent.services.erase(slot); }

		Stub(Stub const& other) = delete;
		auto operator=(Stub const& other) -> Stub& = delete;
		Stub(Stub&& other) = delete;
		auto operator=(Stub&& other) -> Stub& = delete;

	private:
		Locator& parent;
		std::type_index slot;
	};

public:
	// Create and register a service with given arguments
	// A stub is returned, which will destroy the service when it goes out of scope
	template<typename T, typename... Args>
	auto provide(Args&&... args) -> std::pair<T&, Stub>
	{
		auto slot = std::type_index{typeid(T)};
		ASSUME(!services.contains(slot));
		services.emplace(slot, std::make_shared<T>(std::forward<Args>(args)...));
		return {
			std::piecewise_construct,
			std::make_tuple(std::ref(*std::static_pointer_cast<T>(services.at(slot)))),
			std::make_tuple(std::ref(*this), slot)
		};
	}

	// Retrieve an instance of a previously registered service
	template<typename T>
	auto get() -> T&
	{
		auto slot = std::type_index{typeid(T)};
		ASSUME(services.contains(slot));
		return *std::static_pointer_cast<T>(services.at(slot));
	}

	// Replace a previously registered service with a new instance,
	// without affecting its lifetime (the existing stub still applies)
	template<typename T, typename... Args>
	void replace(Args&&... args)
	{
		auto slot = std::type_index{typeid(T)};
		ASSUME(services.contains(slot));
		services.at(slot) = std::make_shared<T>(std::forward<Args>(args)...);
	}

	// Check if a service is available
	template<typename T>
	auto exists() -> bool
	{
		auto slot = std::type_index{typeid(T)};
		return services.contains(slot);
	}

private:
	ankerl::unordered_dense::map<std::type_index, std::shared_ptr<void>> services{};
};

}
