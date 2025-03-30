#pragma once

#include <stdexcept>
#include <utility>

// Service - a scoped singleton
// A service is a class that's globally available, but first an instance of it
// needs to be provided. It's effectively a form of implicit argument passing;
// providing a service allows everything in the same scope to use it.
// If an instance requiring a service is created while the service doesn't
// exist, it will throw.
// An already provided service can be provided again, safely "shadowing"
// the previous one.
//
// Providing a service is done by creating an instance of Service<T>::Provider.
// Access to a provider service is available under Service<T>::serv after
// inheriting Service<T>.
template<typename T>
class Service
{
	using Self = Service<T>;
	using Wrapped = T;

	inline static Wrapped* current_serv = nullptr;

protected:
	Wrapped*& serv = current_serv;

	Service()
	{
		if (!serv)
			throw std::runtime_error("Service requested but not available");
	}

public:
	class Provider
	{
	public:
		template<typename... Args>
		explicit Provider(Args&&... args):
			inst(std::forward<Args>(args)...),
			prev(current_serv)
		{
			current_serv = &inst;
		}

		~Provider()
		{
			if (prev)
				current_serv = prev;
		}

		// Moveable
		Provider(Provider&& other):
			inst(std::move(other.inst)),
			prev(other.prev)
		{
			other.prev = nullptr;
			current_serv = &inst;
		}

		auto operator=(Provider&& other) -> Provider&
		{
			inst = std::move(other.inst);
			prev = other.prev;
			other.prev = nullptr;
			current_serv = &inst;
			return *this;
		}

	private:
		Wrapped inst;
		Wrapped* prev;
	};
};
