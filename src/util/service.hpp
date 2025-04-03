#ifndef PLAYNOTE_UTIL_SERVICE_H
#define PLAYNOTE_UTIL_SERVICE_H

#include <utility>
#include "libassert/assert.hpp"

// Wrapper for providing globally available scoped access to a class instance; singleton on steroids
template<typename T>
class Service {
	class Stub;

public:
	// Create an instance of the underlying service. The service will be destroyed
	// once the returned stub goes out of scope.
	template<typename... Args>
	auto provide(Args&&... args) -> Stub
	{
		return Stub{*this, std::forward<Args>(args)...};
	}

	// Gain access to the currently provisioned instance
	auto operator*() -> T& { return *ASSUME_VAL(handle); }
	auto operator->() -> T* { return ASSUME_VAL(handle); }
	operator bool() const { return handle != nullptr; }

private:
	class Stub {
	public:
		~Stub() { service.handle = prevInstance; }

		template<typename... Args>
		explicit Stub(Service<T>& service, Args&&... args):
			service{service},
			instance{std::forward<Args>(args)...},
			prevInstance{service.handle}
		{
			service.handle = &instance;
		}

		// Not copyable or movable
		Stub(Stub const&) = delete;
		auto operator=(Stub const&) -> Stub& = delete;

	private:
		Service<T>& service;
		T instance;
		T* prevInstance;
	};

	T* handle = nullptr;
};

#endif
