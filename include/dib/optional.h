#ifndef __DIB_OPTIONAL_H
#define __DIB_OPTIONAL_H

#include <utility>
#include <optional>
#include "dib/preprocess.h"
#include "dib/types.h"

namespace dib
{
	struct NoneType
	{
	private:
		constexpr NoneType() {}

	public:
		constexpr static NoneType get() { return {}; }
	};

	inline constexpr NoneType none = NoneType::get();

	template<class T>
	class Optional 
		: public types::TriviallyRelocatableIf<types::is_trivially_relocatable<T>>
	{
		bool _has_value;
		union { T _value; char _dummy; };

	public:
		using value_type = T;

		constexpr static size_t ValueOffset() { return offsetof(Optional<T>, _value); }

		constexpr ~Optional()
		{
			if (_has_value)
			{
				_value.~T();
			}
		}

		constexpr Optional(const Optional &other)
			noexcept(std::is_nothrow_copy_constructible_v<T>)
			: _has_value(other._has_value)
		{
			if (_has_value)
			{
				new(&_value) T(other._value);
			}
		}

		constexpr Optional(Optional &&other)
			noexcept(std::is_nothrow_move_constructible_v<T>)
			: _has_value(other._has_value)
		{
			if (_has_value)
			{
				new(&_value) T(std::move(other._value));
				other._value.~T();
			}

			other._has_value = false;
		}

		constexpr Optional &operator=(const Optional &other)
			noexcept(std::is_nothrow_copy_constructible_v<T> &&std::is_nothrow_copy_assignable_v<T>)
		{
			if (_has_value && other._has_value)
			{
				_value = other._value;
			}
			else if (_has_value && !other._has_value)
			{
				_value.~T();
			}
			else if (!_has_value && other._has_value)
			{
				new(&_value) T(other._value);
			}
			else
			{
				// Nothing needs to happen to copy none to none //
			}

			_has_value = other._has_value;
			return *this;
		}

		constexpr Optional &operator=(Optional &&other)
			noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
		{
			if (_has_value && other._has_value)
			{
				_value = std::move(other._value);
				other._value.~T();
			}
			else if (_has_value && !other._has_value)
			{
				_value.~T();
			}
			else if (!_has_value && other._has_value)
			{
				new(&_value) T(std::move(other._value));
				other._value.~T();
			}
			else
			{
				// Nothing needs to happen to move none to none //
			}

			_has_value = other._has_value;
			other._has_value = false;
			return *this;
		}

		constexpr Optional(NoneType)
			noexcept
			: _has_value(false), _dummy()
		{}

		constexpr Optional() noexcept : Optional(none) {}

		constexpr Optional(const T &inner)
			noexcept(std::is_nothrow_copy_constructible_v<T>)
			: _has_value(true), _value(inner)
		{}

		constexpr Optional(T &&inner)
			noexcept(std::is_nothrow_move_constructible_v<T>)
			: _has_value(true), _value(std::move(inner))
		{}

		template<class... Args> requires (sizeof...(Args) > 1)
		constexpr Optional(Args &&...args)
			noexcept(std::is_nothrow_constructible_v<T, Args...>)
			: _has_value(true), _value(std::forward<Args>(args)...)
		{}

		constexpr operator bool() const { return _has_value; }
		constexpr bool has_value() const { return _has_value; }

		constexpr T &&get() && { return _value; }
		constexpr T &get() & { return _value; }
		constexpr const T &get() const & { return _value; }

		constexpr T &&operator*() &&{ return _value; }
		constexpr T &operator*() & { return _value; }
		constexpr const T &operator*() const & { return _value; }

		constexpr T *operator->() { return &_value; }
		constexpr const T *operator->() const { return &_value; }

		constexpr T get_or(T &&value) &&
		{
			if (_has_value)
			{
				return std::move(_value);
			}

			return std::move(value);
		}

		template<std::invocable Fn>
		constexpr T get_or(Fn &&fn) &&
		{
			if (_has_value)
			{
				return std::move(_value);
			}

			return fn();
		}

		template<std::invocable<T &&> Fn>
		constexpr auto map(Fn &&fn) && -> Optional<std::invoke_result_t<Fn, const T &>>
		{
			if (_has_value)
			{
				return fn(std::move(_value));
			}

			return {};
		}

		template<std::invocable<T &&> Fn>
		constexpr auto then(Fn &&fn) && -> Optional<typename std::invoke_result_t<Fn, const T &>::value_type>
		{
			if (_has_value)
			{
				auto r = fn(std::move(_value));

				if (r) return r;
				else return {};
			}

			return {};
		}

		constexpr auto as_ref() & { return *this ? dib::Optional<T &>(**this) : dib::none; }
		constexpr auto as_ref() const & { return *this ? dib::Optional<const T &>(**this) : dib::none; }
	};

	template<class T>
	class Optional<T &>
	{
		T *_ptr;

	public:
		constexpr static size_t ValueOffset() { return offsetof(Optional<T &>, _ptr); }

		constexpr Optional(NoneType) noexcept
			: _ptr(nullptr)
		{}

		constexpr Optional() noexcept : Optional(none) {}

		constexpr Optional(T &ref) noexcept
			: _ptr(&ref)
		{}

		constexpr operator Optional<const T &>() const
		{
			return _ptr ? Optional<const T &>{*_ptr} : Optional<const T &>{};
		}

		constexpr operator bool() const { return _ptr != nullptr; }
		constexpr bool has_value() const { return _ptr != nullptr; }

		constexpr T &get() { return *_ptr; }
		constexpr const T &get() const { return *_ptr; }

		constexpr T &operator*() { return *_ptr; }
		constexpr const T &operator*() const { return *_ptr; }

		constexpr T *operator->() { return _ptr; }
		constexpr const T *operator->() const { return _ptr; }

		constexpr T get_or(T &value) &&
		{
			if (has_value())
			{
				return *_ptr;
			}

			return value;
		}

		template<std::invocable Fn>
		constexpr T &get_or(Fn &&fn) &&
		{
			if (has_value())
			{
				return *_ptr;
			}

			return fn();
		}

		template<std::invocable<T &> Fn>
		constexpr auto map(Fn &&fn) && -> Optional<std::invoke_result_t<Fn, const T &>>
		{
			if (has_value())
			{
				return fn(*_ptr);
			}

			return {};
		}

		template<std::invocable<T &> Fn>
		constexpr auto then(Fn &&fn) && -> Optional<typename std::invoke_result_t<Fn, const T &>::value_type>
		{
			if (has_value())
			{
				auto r = fn(*_ptr);

				if (r) return std::move(r);
				else return {};
			}

			return {};
		}
	};

	template<class T, class Fn>
	constexpr decltype(auto) operator|(Optional<T> &&opt, Fn &&fn)
	{
		return std::move(opt).map(std::forward<Fn>(fn));
	}

	template<class T, class Fn>
	constexpr decltype(auto) operator||(Optional<T> &&opt, Fn &&fn)
	{
		return std::move(opt).then(std::forward<Fn>(fn));
	}

	template<class T>
	constexpr Optional<T> none_of = {};

	namespace detail
	{
		struct SomeImplDefaultType {};

		template<class T, class V>
		using SomeImplType = std::conditional_t<
			std::is_same_v<T, SomeImplDefaultType>,
			V, T>;
	}

	template<class T = detail::SomeImplDefaultType, class V = T>
	constexpr Optional<detail::SomeImplType<T, V>> some(V &&val)
	{
		return { std::forward<V>(val) };
	}
}

#endif