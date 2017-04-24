#pragma once

/// Implementation of floating-point number (float or double, depending on default code precision) 
/// with atomic operators, defined using compare-exchange.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class AtomicFloat {
private:
	std::atomic<Float> value;
	
public:
	AtomicFloat() = default;
	
	AtomicFloat(const Float f) {
		value = f;
	}
	
	AtomicFloat& operator=(const Float f) {
		atomicOp(f, [](const Float, const Float rhs){
			return rhs;
		});
		return *this;
	}
	
	AtomicFloat& operator+=(const Float f) {
		atomicOp(f, [](const Float lhs, const Float rhs){
			return lhs + rhs;
		});
		return *this;
	}
	
	AtomicFloat& operator-=(const Float f) {
		atomicOp(f, [](const Float lhs, const Float rhs){
			return lhs - rhs;
		});
		return *this;
	}
	
	AtomicFloat& operator*=(const Float f) {
		atomicOp(f, [](const Float lhs, const Float rhs){
			return lhs * rhs;
		});
		return *this;
	}
	
	AtomicFloat& operator/(const Float f) {
		ASSERT(f != 0._f);
		atomicOp(f, [](const Float lhs, const Float rhs){
			return lhs / rhs;
		});
		return *this;
	}
	
	Float operator+(const Float f) const {
		return value.load() + f;
	}
	
	Float operator-(const Float f) const {
		return value.load() - f;
	}
	
	Float operator*(const Float f) const {
		return value.load() * f;
	}
	
	Float operator/(const Float f) const {
		ASSERT(f != 0._f);
		return value.load() / f;
	}

private:
	template<typename TOp>
	Float atomicOp(const Float rhs, const TOp& op) {
		Float lhs       =  value.load();
		Float desired   =  op(lhs, rhs);
		while (!value.compare_exchange_weak(lhs, desired)) {
			desired  =  op(lhs, rhs);
		}
		return desired;
	}
};

NAMESPACE_SPH_END