#pragma once
#include <type_traits>
#include <memory>	//for std::addressof
#include "core.h"

template <class T>
class RefWrapper {
	T* ptr=nullptr;
public:
	static_assert(std::is_reference<T>::value, "RefWrapper should be used for reference only.");
	using type=T;
	
	RefWrapper() = default;
	RefWrapper(T ref) noexcept : ptr(std::addressof(ref)) {}
	RefWrapper(const RefWrapper&) noexcept = default;
	RefWrapper(RefWrapper&&) noexcept = default;
	
	RefWrapper& operator=(const RefWrapper& x) noexcept = default;
	RefWrapper& operator=(RefWrapper&& x) noexcept = default;
	
	operator T () const noexcept { return forward_m(*ptr); }
	T get() const noexcept { return forward_m(*ptr); }
	void reset(T ref) {
		ptr = &ref;
	}
};
template<class T>
struct OptionalVal {
	union {
		T val;
		bool dummy;
	}val;
	bool is_active=false;
	OptionalVal() = default;
	void operator=(OptionalVal) = delete;
	OptionalVal(OptionalVal&&) = delete;

	template<class Iter,class...Ts>
	void reset(const Iter&iter,Ts&&...args) {
		if(!is_active)
			val.val.~T();
		new(&(val.val)) T( (*iter)(forward_m(args)...) );
		is_active = true;
	}
	
	explicit operator bool()const noexcept {
		return is_active;
	}
	void reset()noexcept {
		if(is_active)
			val.val.~T();
	}
	const T& get()const noexcept{
		return val.val;
	}
	T& get()noexcept{
		return val.val;
	}
	~OptionalVal() {
		reset();
	}
};

template<class R>
struct CacheRes:public OptionalVal<R> {
	 //todo: we shall use a std::optional<R> instead of unique_ptr.
	using Base = OptionalVal<R>;
	using reference_type	= R&;
	using value_type		= R;
	using pointer			= R*;
};

template<>
struct CacheRes<void>{
	using reference_type	= const CacheRes<void>&;
	using value_type		= const CacheRes<void>;
	using pointer			= const CacheRes<void>*;
	bool is_called=false;
	template<class Iter,class...Ts>
	void reset(const Iter&iter,Ts&&...args) { 
		iter->lock_then_call(
			[&args...](auto&& slot) {
				forward_m(slot)(forward_m(args)...); 
			}
		);
		is_called = true;
	}
	void reset()noexcept{
		is_called = false;
	}
	explicit operator bool()const noexcept {
		return is_called;
	}
	const CacheRes<void>& get()const noexcept{
		return *this;
	}
};
template<class R>
class CacheRes<R&&>: public RefWrapper<R&&>{ };
