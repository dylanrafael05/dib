#pragma once

#include <utility>

#include "dib/preprocess.h"
#include "dib/record.h"
#include "dib/types.h"
#include "dib/debug.h"

namespace dib::option
{
	/// Define a helper type which is used to initialize an
	/// option to 'none'
	struct NoneType
	{
	private:
		constexpr NoneType() {}

	public:
		constexpr static NoneType get() { return {}; }
	};

	inline constexpr auto none = NoneType::get();

	/// A wrapper around a raw pointer which prevents users from
	/// dereferencing a null value.
	template<class T>
	class [[=hash_as_record, =compare_as_record]] SafePointer
	{
		T *_ptr;

	public:
		constexpr SafePointer(T *ptr) : _ptr(ptr) {}

		constexpr operator bool() const { return _ptr != nullptr; }
		constexpr T &operator*() const { return *operator->(); }
		constexpr T *operator->() const
		{ 
			if(_ptr == nullptr) [[unlikely]]
				RUNTIME_ERROR("Attempt to access value of null SafePointer");

			return _ptr; 
		}
	};

	/// A type which holds either a value or the absence of one
	template<class T>
	class [[=provides_hash]] Option 
		: public types::TriviallyRelocatableIf<types::is_trivially_relocatable<T>>
	{
		union { T _value; char _dummy; };
		bool _has_value;

	public:
		using value_type = T;

		constexpr static size_t ValueOffset() { return offsetof(Option<T>, _value); }

		constexpr ~Option()
		{
			if (_has_value)
			{
				_value.~T();
			}
		}

		constexpr Option(const Option &other)
			noexcept(std::is_nothrow_copy_constructible_v<T>)
			: _has_value(other._has_value)
		{
			if (_has_value)
			{
				new(&_value) T(other._value);
			}
		}

		constexpr Option(Option &&other)
			noexcept(std::is_nothrow_move_constructible_v<T>)
			: _has_value(other._has_value)
		{
			if (_has_value)
			{
				new(&_value) T(MOVE(other._value));
				other._value.~T();
			}

			other._has_value = false;
		}

		constexpr Option &operator=(const Option &other)
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

		constexpr Option &operator=(Option &&other)
			noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
		{
			if (_has_value && other._has_value)
			{
				_value = MOVE(other._value);
				other._value.~T();
			}
			else if (_has_value && !other._has_value)
			{
				_value.~T();
			}
			else if (!_has_value && other._has_value)
			{
				new(&_value) T(MOVE(other._value));
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

		constexpr Option(NoneType)
			noexcept
			: _dummy(), _has_value(false)
		{}

		constexpr Option() noexcept : Option(none) {}

		constexpr Option(const T &inner)
			noexcept(std::is_nothrow_copy_constructible_v<T>)
			: _value(inner), _has_value(true)
		{}

		constexpr Option(T &&inner)
			noexcept(std::is_nothrow_move_constructible_v<T>)
			: _value(MOVE(inner)), _has_value(true)
		{}

		template<class... Args> requires (sizeof...(Args) > 1)
		constexpr Option(Args &&...args)
			noexcept(std::is_nothrow_constructible_v<T, Args...>)
			: _value(std::forward<Args>(args)...), _has_value(true)
		{}

		constexpr operator bool() const { return _has_value; }
		constexpr bool has_value() const { return _has_value; }
		
		constexpr SafePointer<T> 	   try_get()       { return _has_value ? &_value : nullptr; }
		constexpr SafePointer<const T> try_get() const { return _has_value ? &_value : nullptr; }

		template<class Self>
		constexpr decltype(auto) unwrap(this Self &&self)
		{ 
			if(!self.has_value()) 
				RUNTIME_ERROR("Unwrapping a 'none'"); 

			return static_cast<types::CopyConstRef<Self &&, T>>((T &)(self._value)); 
		}

		constexpr T unwrap_or(T &&value) &&
		{
			if (_has_value)
			{
				return MOVE(_value);
			}

			return MOVE(value);
		}

		template<std::invocable Fn> requires types::NotSameAs<T, Fn>
		constexpr T unwrap_or(Fn &&fn) &&
		{
			if (_has_value)
			{
				return MOVE(_value);
			}

			return fn();
		}

		template<std::invocable<T &&> Fn>
		constexpr auto map(Fn &&fn) && -> Option<std::invoke_result_t<Fn, T &&>>
		{
			if (_has_value)
			{
				return std::invoke(fn, MOVE(_value));
			}

			return {};
		}

		template<std::invocable<T &&> Fn>
		constexpr auto then(Fn &&fn) && -> Option<typename std::invoke_result_t<Fn, T &&>::value_type>
		{
			if (_has_value)
			{
				auto r = std::invoke(fn, MOVE(_value));

				if (r) return r;
				else return {};
			}

			return {};
		}

		constexpr auto ref() & { return *this ? Option<T &>(unwrap()) : none; }
		constexpr auto ref() const & { return *this ? Option<const T &>(unwrap()) : none; }

		constexpr bool operator==(const Option<T> &other) const requires types::IsEqualityComparable<T>
		{
			if(!has_value()) return !other.has_value();
			if(!other.has_value()) return !has_value();

			return unwrap() == other.unwrap();
		}

		constexpr size_t get_hash() const requires types::IsHashable<T>
		{
			if(!has_value())
				return 0;

			return dib::get_hash(unwrap());
		}
	};

	/// Specialization to handle references
	template<class T>
	class Option<T &>
	{
		T *_ptr;

	public:
		constexpr static size_t ValueOffset() { return offsetof(Option<T &>, _ptr); }

		constexpr Option(NoneType) noexcept
			: _ptr(nullptr)
		{}

		constexpr Option() noexcept : Option(none) {}

		constexpr Option(T &ref) noexcept
			: _ptr(std::addressof(ref))
		{}

		constexpr operator Option<const T &>() const
		{
			return _ptr ? Option<const T &>{*_ptr} : Option<const T &>{};
		}

		constexpr operator bool() const { return _ptr != nullptr; }
		constexpr bool has_value() const { return _ptr != nullptr; }
		
		constexpr SafePointer<T> 	   try_get()       { return _ptr; }
		constexpr SafePointer<const T> try_get() const { return _ptr; }

		template<class Self>
		constexpr decltype(auto) unwrap(this Self &&self) 
		{ 
			if(!self._ptr) 
				RUNTIME_ERROR("Unwrapping none"); 
			return *self._ptr; 
		}

		constexpr T &unwrap_or(T &value) &&
		{
			if (has_value())
			{
				return *_ptr;
			}

			return value;
		}

		template<std::invocable Fn>
		constexpr T &unwrap_or(Fn &&fn) &&
		{
			if (has_value())
			{
				return *_ptr;
			}

			return fn();
		}

		template<std::invocable<T &> Fn>
		constexpr auto map(Fn &&fn) && -> Option<std::invoke_result_t<Fn, const T &>>
		{
			if (has_value())
			{
				return std::invoke(fn, *_ptr);
			}

			return {};
		}

		template<std::invocable<T &> Fn>
		constexpr auto then(Fn &&fn) && -> Option<typename std::invoke_result_t<Fn, const T &>::value_type>
		{
			if (has_value())
			{
				auto r = std::invoke(fn, *_ptr);

				if (r) return MOVE(r);
				else return {};
			}

			return {};
		}
	};

	template<class T, class Fn>
	constexpr decltype(auto) operator|(Option<T> &&opt, Fn &&fn)
	{
		return MOVE(opt).map(FORWARD(fn));
	}

	template<class T, class Fn>
	constexpr decltype(auto) operator||(Option<T> &&opt, Fn &&fn)
	{
		return MOVE(opt).then(FORWARD(fn));
	}

	template<class T>
	constexpr Option<T> none_of = {};

	namespace detail
	{
		struct SomeImplDefaultType {};

		template<class T, class V>
		using SomeImplType = std::conditional_t<
			std::is_same_v<T, SomeImplDefaultType>,
			V, T>;
	}

	template<class T = detail::SomeImplDefaultType, class V = T>
	constexpr Option<detail::SomeImplType<T, V>> some(V &&val)
	{
		return { std::forward<V>(val) };
	}
}

// NOTE; we have to define this here because otherwise we get into a dependency loop
namespace dib
{
	template<class T>
    consteval option::Option<T> annotation(std::meta::info refl, bool include_bases=true)
    {
        for(auto annotation : std::meta::annotations_of(refl))
        {
            if(std::meta::is_assignable_type(^^T, std::meta::type_of(annotation)))
            {
                return std::meta::extract<T>(annotation);
            }
        }

        if(include_bases && std::meta::is_type(refl) && std::meta::is_class_type(refl))
        {
            for(auto base : std::meta::bases_of(refl, std::meta::access_context::unprivileged()))
            {
                auto partial = annotation<T>(std::meta::type_of(base));
                if(partial)
                    return partial;
            }
        }

        return option::none;
    }
}