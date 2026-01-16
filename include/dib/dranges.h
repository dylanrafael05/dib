#pragma once

#include <concepts>
#include <utility>
#include <functional>
#include <ranges>

#include "dib/option.h"

namespace dib::dranges
{
    /// A helper type which can be assigned to anything and does
    /// nothing.
    struct Absorber
    {
        template<class T>
        Absorber &operator=(T &&) {return *this;}
    };

    /// A forward iterator which counts how many times it has been incremented,
    /// and which returns an absorber
    struct CountingIterator
    {
        size_t count;

        CountingIterator &operator++()
        {
            count++;
            return *this;
        }

        CountingIterator operator++(int)
        {
            auto copy = *this;
            count++;
            return copy;
        }

        Absorber operator*() const {return {};}
    };

	namespace detail
	{
		template<class T> struct OptionalHelperType : std::false_type {};
		template<class T> struct OptionalHelperType<dib::option::Option<T>> : std::true_type { using type = T; };

		template<class T> concept IsOptional = OptionalHelperType<T>::value;
		template<class T> using UnwrapOptional = OptionalHelperType<T>::type;

		template<class T>
		class LambdaWrapper
		{
		public:
			constexpr LambdaWrapper()
				: value(dib::option::none)
			{}

			constexpr LambdaWrapper(const T &value) requires std::is_copy_constructible_v<T>
				: value(value)
			{}

			constexpr LambdaWrapper(T &&value) requires std::is_move_constructible_v<T>
				: value(MOVE(value))
			{}

			constexpr LambdaWrapper(const LambdaWrapper &) = default;
			constexpr LambdaWrapper(LambdaWrapper &&) = default;

			constexpr LambdaWrapper &operator=(const LambdaWrapper &other) requires std::is_copy_constructible_v<T>
			{
				// TODO: is this possible in any other way? //
				// Currently, this nukes constexpr support, but is also //
				// necessary to maintain forward iterator support. //
				value.~Option();
				new (&value) dib::option::Option<T>(other.value);

				return *this;
			}

			constexpr LambdaWrapper &operator=(LambdaWrapper &&other) requires std::is_move_constructible_v<T>
			{
				value.~Option();
				new (&value) dib::option::Option<T>(MOVE(other.value));

				return *this;
			}

			dib::option::Option<T> value;
		};

		template<class T> struct ReferenceWrapHelper
		{
			using type = T;

			static decltype(auto) wrap(auto &&val) { return static_cast<decltype(val)>(val); }
			static decltype(auto) unwrap(auto &&val) { return static_cast<decltype(val)>(val); }
		};

		template<class T> struct ReferenceWrapHelper<T &>
		{
			using type = T *;

			static T *wrap(T &val) { return &val; }
			static T &unwrap(T *val) { return *val; }
		};

		template<class T> struct ReferenceWrapHelper<T &&>
		{
			using type = T *;

			static T *wrap(T &&val) { return &val; }
			static T &&unwrap(T *val) { return MOVE(*val); }
		};

		struct GenerateEndSentinel {};

		template<std::invocable Generator>
			requires IsOptional<std::invoke_result_t<Generator>>
		class InputGenerateIterator
		{
			using result_t = std::invoke_result_t<Generator>;
			using value_t = UnwrapOptional<result_t>;

			constexpr static bool is_reference = std::is_reference_v<value_t>;
			using arg_t = std::conditional_t<is_reference,
				value_t,
				value_t &&>;

			Generator *_gen;
			mutable result_t _val;

		public:
			constexpr InputGenerateIterator(arg_t val, Generator *g)
				: _gen(g), _val(std::forward<arg_t>(val))
			{}

			constexpr InputGenerateIterator(Generator *g)
				: _gen(g), _val(std::invoke(*_gen))
			{}

			using value_type = std::remove_reference_t<value_t>;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::input_iterator_tag;

			constexpr decltype(auto) operator*() const { return _val.unwrap(); }
			constexpr decltype(auto) operator->() const { return &_val.unwrap(); }

			constexpr InputGenerateIterator &operator++()
			{
				_val = std::invoke(*_gen);
				return *this;
			}

			constexpr void operator++(int)
			{
				operator++();
			}

			constexpr bool operator==(GenerateEndSentinel) const { return !_val.has_value(); }
		};

		template<std::invocable Generator>
			requires IsOptional<std::invoke_result_t<Generator>>
		class ForwardGenerateIterator
		{
			using result_t = std::invoke_result_t<Generator>;
			using value_t = UnwrapOptional<result_t>;

			constexpr static bool is_reference = std::is_reference_v<value_t>;
			using arg_t = std::conditional_t<is_reference,
				value_t,
				value_t &&>;

			LambdaWrapper<Generator> _gen;
			mutable result_t _val;
			size_t _call_counter = 0;

		public:
			constexpr ForwardGenerateIterator()
				: _gen(), _val()
			{}

			constexpr ForwardGenerateIterator(arg_t val, const Generator &g)
				: _gen(g), _val(std::forward<arg_t>(val))
			{}

			constexpr ForwardGenerateIterator(const Generator &g)
				: _gen(g), _val(std::invoke(_gen.value.unwrap()))
			{}

			using value_type = std::remove_reference_t<value_t>;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::forward_iterator_tag;

			constexpr decltype(auto) operator*() const { return _val.unwrap(); }
			constexpr decltype(auto) operator->() const { return &_val.unwrap(); }

			constexpr ForwardGenerateIterator &operator++()
			{
				_val = std::invoke(*_gen.value);
				_call_counter++;
				return *this;
			}

			constexpr ForwardGenerateIterator operator++(int)
			{
				auto cpy = *this;
				operator++();
				return cpy;
			}

			constexpr bool operator==(GenerateEndSentinel) const { return !_val.has_value(); }

			constexpr bool operator==(const ForwardGenerateIterator &other) const { return _call_counter == other._call_counter; }
			constexpr bool operator!=(const ForwardGenerateIterator &other) const { return _call_counter != other._call_counter; }
		};

		template<std::invocable Generator, bool Input>
			requires IsOptional<std::invoke_result_t<Generator>>
		class GenerateOptionalRange
		{
			Generator _gen;

			using iter = std::conditional_t<
				Input,
				InputGenerateIterator<Generator>,
				ForwardGenerateIterator<Generator>>;

		public:
			constexpr explicit GenerateOptionalRange(Generator &&g)
				: _gen(MOVE(g))
			{}

			constexpr iter begin()
			{
				if constexpr (Input) return { &_gen }; else return { _gen };
			}

			constexpr GenerateEndSentinel end()
			{
				return {};
			}
		};

		template<std::invocable Generator, bool Input>
			requires IsOptional<std::invoke_result_t<Generator>>
		class GenerateOptionalRangeWithStart
		{
			using val_t = UnwrapOptional<std::invoke_result_t<Generator>>;
			using wrapper = ReferenceWrapHelper<val_t>;
			using wrap_t = wrapper::type;

			Generator _gen;
			mutable wrap_t _val;

			using iter = std::conditional_t<
				Input,
				InputGenerateIterator<Generator>,
				ForwardGenerateIterator<Generator>>;

		public:
			template<class Start>
			constexpr explicit GenerateOptionalRangeWithStart(Start &&val, Generator &&g)
				: _gen(MOVE(g)), _val(wrapper::wrap(std::forward<Start>(val)))
			{}

			constexpr iter begin()
			{
				if constexpr (Input) return { wrapper::unwrap(MOVE(_val)), &_gen };
				else return { wrapper::unwrap(MOVE(_val)), _gen };
			}

			constexpr GenerateEndSentinel end()
			{
				return {};
			}
		};

		static_assert(std::ranges::forward_range<GenerateOptionalRange<dib::option::Option<int>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRange<dib::option::Option<int &>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRangeWithStart<dib::option::Option<int>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRangeWithStart<dib::option::Option<int &>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::input_range<GenerateOptionalRange<dib::option::Option<int>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRange<dib::option::Option<int &>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRangeWithStart<dib::option::Option<int>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRangeWithStart<dib::option::Option<int &>(*)(), true>>,
			"Generated ranges should satisfy input_range");
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::input_range, and cannot be iterated over multiple times.
	/// </summary>
	template<std::invocable Generator>
	requires detail::IsOptional<std::invoke_result_t<Generator>>
	constexpr auto generate(Generator &&g)
	{
		return detail::GenerateOptionalRange<Generator, true>{ MOVE(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function after some starting value, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::input_range, and cannot be iterated over multiple times.
	/// </summary>
	template<std::invocable Generator, class Start>
	requires detail::IsOptional<std::invoke_result_t<Generator>>
		&& std::convertible_to<Start, detail::UnwrapOptional<std::invoke_result_t<Generator>>>
	constexpr auto generate(Start &&start, Generator &&g)
	{
		return detail::GenerateOptionalRangeWithStart<Generator, true>{ std::forward<Start>(start), MOVE(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::forward_range, and thus the function must be deterministic for
	/// any set of inputs as well as being copy-constructible.
	/// </summary>
	template<std::invocable Generator>
	requires detail::IsOptional<std::invoke_result_t<Generator>>
	constexpr auto generate_multi(Generator &&g)
	{
		return detail::GenerateOptionalRange<Generator, false>{ MOVE(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function and the provided starting value, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::forward_range, and thus the function must be deterministic for
	/// any set of inputs as well as being copy-constructible.
	/// </summary>
	template<std::invocable Generator, class Start>
	requires detail::IsOptional<std::invoke_result_t<Generator>>
		&& std::convertible_to<Start, detail::UnwrapOptional<std::invoke_result_t<Generator>>>
	constexpr auto generate_multi(Start &&start, Generator &&g)
	{
		return detail::GenerateOptionalRangeWithStart<Generator, false>{ std::forward<Start>(start), MOVE(g) };
	}

	namespace detail
	{
		template<class Container>
		struct ToFn_Typename
		{};

		template<template<class...> class Container>
		struct ToFn_Template
		{};

		template<class T>
		concept has_reserve = requires(T & t, size_t sz) { t.reserve(sz); };

		template<class Container, std::ranges::input_range Rng>
		constexpr Container to_impl(Rng &&range)
		{
			Container out{};
			
			if constexpr (std::ranges::sized_range<Rng> && has_reserve<Container>)
			{
				out.reserve(std::ranges::size(range));
			}

			for (auto it = range.begin(); it != range.end(); it++)
			{
				out.push_back(std::ranges::iter_move(it));
			}

			return out;
		}

		template<std::ranges::range Rng, class Container>
		constexpr decltype(auto) operator|(Rng &&range, ToFn_Typename<Container>)
		{
			return to_impl<Container>(range);
		}

		template<std::ranges::range Rng, template<class...> class Container>
		constexpr decltype(auto) deduce_helper(Rng &&range)
		{
			using Rval = std::ranges::range_value_t<Rng>;

			if constexpr (requires { Container(range); })
			{
				return Container(range);
			}
			else if constexpr (requires { Container((Rval *)nullptr, (Rval *)nullptr); })
			{
				return Container((Rval *)nullptr, (Rval *)nullptr);
			}
			else
			{
				static_assert(!std::is_same_v<Rng, Rng>, "Unsupported collection!");
			}
		}

		template<std::ranges::range Rng, template<class...> class Container>
		constexpr decltype(auto) operator|(Rng &&range, ToFn_Template<Container>)
		{
			using Cont = decltype(deduce_helper<Rng, Container>(std::forward<Rng>(range)));
			return to_impl<Cont>(range);
		}
	}
	
	template<class Container> 
	constexpr detail::ToFn_Typename<Container> to() { return {}; }
	template<template<class...> class Container>
	constexpr detail::ToFn_Template<Container> to() { return {}; }

	namespace detail
	{
		struct EnumerateFn {};

		template<std::ranges::range Rng>
		auto operator|(Rng &&range, EnumerateFn)
		{
			return range | std::ranges::views::transform([i = (size_t)0](auto &&value) mutable
			{
				if constexpr (std::ranges::borrowed_range<Rng>)
				{
					return std::tuple<decltype(value), size_t>(std::forward<decltype(value)>(value), i++);
				}
				else
				{
					return std::tuple<std::remove_cvref_t<decltype(value)>, size_t>(value, i++);
				}
			});
		}
	}

	constexpr detail::EnumerateFn enumerate() { return {}; }

	template<class B, class E>
	constexpr auto iterrange(B &&begin, E &&end)
	{
		class Range
		{
			std::remove_reference_t<B> _begin;
			std::remove_reference_t<E> _end;

		public:
			Range(B &&begin, E &&end) : _begin(FORWARD(begin)), _end(FORWARD(end))
			{}

			auto begin() const { return _begin; }
			auto end() const { return _end; }
		};

		return Range(FORWARD(begin), FORWARD(end));
	}
}