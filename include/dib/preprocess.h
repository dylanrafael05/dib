#ifndef __DIB_PREPROCESSOR_H
#define __DIB_PREPROCESSOR_H

#include <utility>

#define DIB_DESTRUCT __destructor
#define DIB_COPY_FROM __copy_from
#define DIB_MOVE_FROM __move_from

#define DIB_RULE_OF_5_DECL(T) \
	T(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))); \
	T(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value)))); \
	~T(); \
	T &operator=(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))); \
	T &operator=(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value))));


#define DIB_RULE_OF_5_IMPL(T) \
	T::T(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))) {this->DIB_COPY_FROM(value);} \
	T::T(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value)))) {this->DIB_MOVE_FROM(::std::move(value));} \
	~T::T() {this->DIB_DESTRUCT();} \
	T::T &operator=(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))) {if(&value == this) [[unlikely]] return *this; this->DIB_DESTRUCT(); this->DIB_COPY_FROM(value); return *this;} \
	T::T &operator=(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value)))) {if(&value == this) [[unlikely]] return *this; this->DIB_DESTRUCT(); this->DIB_MOVE_FROM(::std::move(value)); return *this;}


#define DIB_RULE_OF_5(T) \
	T(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))) {this->DIB_COPY_FROM(value);} \
	T(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value)))) {this->DIB_MOVE_FROM(::std::move(value));} \
	~T() {this->DIB_DESTRUCT();} \
	T &operator=(const T &value) noexcept(noexcept(DIB_COPY_FROM(value))) {if(&value == this) [[unlikely]] return *this; this->DIB_DESTRUCT(); this->DIB_COPY_FROM(value); return *this;} \
	T &operator=(T &&value) noexcept(noexcept(DIB_MOVE_FROM(::std::move(value)))) {if(&value == this) [[unlikely]] return *this; this->DIB_DESTRUCT(); this->DIB_MOVE_FROM(::std::move(value)); return *this;}

#define DIB_FWD(x) static_cast<decltype(x)>(x)
#define DIB_MOV(x) static_cast<::std::remove_reference_t<decltype(x)> &&>(x)

#endif