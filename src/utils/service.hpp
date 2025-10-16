/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

service.hpp:
A wrapper for existing classes to make them into lifetime-managed singletons.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"

namespace playnote {

// A wrapper for RAII-managed global services.
template<typename T>
class Service {
	class Stub;
public:

	// Create an instance of the underlying service. The service will be destroyed once
	// the returned stub goes out of scope.
	template<typename... Args>
	auto provide(Args&&... args) -> Stub {
		return Stub(*this, forward<Args>(args)...);
	}

	// Gain access to the currently provisioned instance
	auto operator*() -> T& { return *ASSUME_VAL(handle); }
	auto operator->() -> T* { return ASSUME_VAL(handle); }

	// Check if instance exists
	explicit operator bool() const { return handle != nullptr; }

private:
	class Stub {
	public:
		~Stub() { service.handle = prev_instance; }

		template<typename... Args>
		Stub(Service<T>& service, Args&&... args):
			service(service),
			instance(std::forward<Args>(args)...),
			prev_instance(service.handle) {
			service.handle = &instance;
		}

		// Not copyable or movable
		Stub(Stub const&) = delete;
		auto operator=(Stub const&)->Stub & = delete;

	private:
		Service<T>& service;
		T instance;
		T* prev_instance;
	};

	T* handle{nullptr};
};

}
