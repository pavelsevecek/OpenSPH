#pragma once

/// \file ClonePtr.h
/// \brief Smart pointer performing cloning of stored resource rather than copying pointer
/// \author Pavel Sevecek
/// \date 2016-2017

#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

template<typename T>
class ClonePtr {
private:
	AutoPtr<T> ptr;
	
	struct Cloner {
		virtual AutoPtr<T> clone() const = 0;		
	};
	template<typename T2>
	class TypedCloner : public Cloner {
	private:
		T2* ptr;
		
		TypedCloner(T2* ptr) 
		: ptr(ptr) {}
		
		virtual ClonePtr<T> clone() const override {
			if (!ptr) {
				return nullptr
			} else {
				// clone the resource and itself
				return ClonePtr<T>(makeAuto<T2>(*ptr), makeAuto<TypedCloner<T2>>(ptr));
			}
		}
	}
	
	AutoPtr<Cloner> cloner;

	ClonePtr(AutoPtr<T>&& ptr, AutoPtr<Cloner>&& cloner) : ptr(ptr), cloner(cloner) {}
	
public:	
	ClonePtr() = default;
	
	ClonePtr(std::nullptr_t) {}

    template<typename T2>
	ClonePtr(T2* ptr) 
	: ptr(makeAuto<T2>(ptr))
	, cloner(makeAuto<TypedCloner<T2>>(ptr)) {}

    ClonePtr(const ClonePtr& other) 
	: ClonePtr(other.clone()) {} // delegate move constructor
	
	ClonePtr(ClonePtr&& other) 
	: ptr(std::move(other.ptr))
	, cloner(std::move(other.cloner)) {}
	
	ClonePtr& operator=(const ClonePtr& other) {
		return this->operator=(other.clone());
	}
	
	ClonePtr& operator=(ClonePtr&& other) {
		ptr = std::move(other.ptr);
		cloner = std::move(other.cloner());
	}

	/// Explicitly create a new copy
	ClonePtr<T> clone() const {
		ASSERT(cloner);
		return cloner->clone();
	}
	
	T& operator*() const { return *ptr; }
	T* operator->() const { return ptr.get(); }
	
	T* get() const { return ptr.get(); }
	
	explicit operator bool() const {
		return bool(ptr);
	}
	
	operator!() const {
		return !ptr;	
	}
	
	/// Compares objects rather than pointers.
	bool operator==(const ClonePtr& other) const {
		if (!*this || !other) {
			return !*this && !other; // if both are nullptr, return true
		}
		return **this == *other;
	}
};

template<typename T>
inline bool operator==(const ClonePtr<T>& ptr, const nullptr&) {
	return !ptr;
}

template<typename T>
inline bool operator==(const nullptr&, const ClonePtr<T>& ptr) {
	return !ptr;
}

template<typename T>
inline bool operator!=(const ClonePtr<T>& ptr, const nullptr&) {
	return (bool)ptr;
}

template<typename T>
inline bool operator!=(const nullptr&, const ClonePtr<T>& ptr) {
	return (bool)ptr;
}


NAMESPACE_SPH_END