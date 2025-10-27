#ifndef __DIB_DRANGES_H
#define __DIB_DRANGES_H

#include <vector>
#include <concepts>
#include <unordered_map>
#include <list>
#include <ranges>
#include <utility>
#include <functional>
#include "dib/optional.h"

namespace dib::dranges
{
	namespace detail
	{
		template<class T> struct __optional_helper : std::false_type {};
		template<class T> struct __optional_helper<dib::Optional<T>> : std::true_type { using type = T; };

		template<class T> concept is_optional = __optional_helper<T>::value;
		template<class T> using optional_unwrap_t = __optional_helper<T>::type;

		template<class T>
		class LambdaWrapper
		{
		public:
			constexpr LambdaWrapper()
				: value(dib::none)
			{}

			constexpr LambdaWrapper(const T &value) requires std::is_copy_constructible_v<T>
				: value(value)
			{}

			constexpr LambdaWrapper(T &&value) requires std::is_move_constructible_v<T>
				: value(std::move(value))
			{}

			constexpr LambdaWrapper(const LambdaWrapper &) = default;
			constexpr LambdaWrapper(LambdaWrapper &&) = default;

			constexpr LambdaWrapper &operator=(const LambdaWrapper &other) requires std::is_copy_constructible_v<T>
			{
				// TODO: is this possible in any other way? //
				// Currently, this nukes constexpr support, but is also //
				// necessary to maintain forward iterator support. //
				value.~Optional();
				new (&value) dib::Optional<T>(other.value);

				return *this;
			}

			constexpr LambdaWrapper &operator=(LambdaWrapper &&other) requires std::is_move_constructible_v<T>
			{
				value.~Optional();
				new (&value) dib::Optional<T>(std::move(other.value));

				return *this;
			}

			dib::Optional<T> value;
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
			static T &&unwrap(T *val) { return std::move(*val); }
		};

		struct GenerateEndSentinel {};

		template<std::invocable Generator>
			requires is_optional<std::invoke_result_t<Generator>>
		class InputGenerateIterator
		{
			using result_t = std::invoke_result_t<Generator>;
			using value_t = optional_unwrap_t<result_t>;

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

			constexpr decltype(auto) operator*() const { return *_val; }
			constexpr decltype(auto) operator->() const { return _val.operator->(); }

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
			requires is_optional<std::invoke_result_t<Generator>>
		class ForwardGenerateIterator
		{
			using result_t = std::invoke_result_t<Generator>;
			using value_t = optional_unwrap_t<result_t>;

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
				: _gen(g), _val(std::invoke(*_gen.value))
			{}

			using value_type = std::remove_reference_t<value_t>;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::forward_iterator_tag;

			constexpr decltype(auto) operator*() const { return *_val; }
			constexpr decltype(auto) operator->() const { return _val.operator->(); }

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
			requires is_optional<std::invoke_result_t<Generator>>
		class GenerateOptionalRange
		{
			Generator _gen;

			using iter = std::conditional_t<
				Input,
				InputGenerateIterator<Generator>,
				ForwardGenerateIterator<Generator>>;

		public:
			constexpr explicit GenerateOptionalRange(Generator &&g)
				: _gen(std::move(g))
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
			requires is_optional<std::invoke_result_t<Generator>>
		class GenerateOptionalRangeWithStart
		{
			using val_t = optional_unwrap_t<std::invoke_result_t<Generator>>;
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
				: _gen(std::move(g)), _val(wrapper::wrap(std::forward<Start>(val)))
			{}

			constexpr iter begin()
			{
				if constexpr (Input) return { wrapper::unwrap(std::move(_val)), &_gen };
				else return { wrapper::unwrap(std::move(_val)), _gen };
			}

			constexpr GenerateEndSentinel end()
			{
				return {};
			}
		};

		static_assert(std::ranges::forward_range<GenerateOptionalRange<dib::Optional<int>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRange<dib::Optional<int &>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRangeWithStart<dib::Optional<int>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::forward_range<GenerateOptionalRangeWithStart<dib::Optional<int &>(*)(), false>>,
			"Generated ranges should satisfy forward_range");

		static_assert(std::ranges::input_range<GenerateOptionalRange<dib::Optional<int>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRange<dib::Optional<int &>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRangeWithStart<dib::Optional<int>(*)(), true>>,
			"Generated ranges should satisfy input_range");

		static_assert(std::ranges::input_range<GenerateOptionalRangeWithStart<dib::Optional<int &>(*)(), true>>,
			"Generated ranges should satisfy input_range");
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::input_range, and cannot be iterated over multiple times.
	/// </summary>
	template<std::invocable Generator>
	requires detail::is_optional<std::invoke_result_t<Generator>>
	constexpr auto generate(Generator &&g)
	{
		return detail::GenerateOptionalRange<Generator, true>{ std::move(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function after some starting value, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::input_range, and cannot be iterated over multiple times.
	/// </summary>
	template<std::invocable Generator, class Start>
	requires detail::is_optional<std::invoke_result_t<Generator>>
		&& std::convertible_to<Start, detail::optional_unwrap_t<std::invoke_result_t<Generator>>>
	constexpr auto generate(Start &&start, Generator &&g)
	{
		return detail::GenerateOptionalRangeWithStart<Generator, true>{ std::forward<Start>(start), std::move(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::forward_range, and thus the function must be deterministic for
	/// any set of inputs as well as being copy-constructible.
	/// </summary>
	template<std::invocable Generator>
	requires detail::is_optional<std::invoke_result_t<Generator>>
	constexpr auto generate_multi(Generator &&g)
	{
		return detail::GenerateOptionalRange<Generator, false>{ std::move(g) };
	}

	/// <summary>
	/// Create a range object which lazily calculates its values by
	/// repeatedly calling the provided function and the provided starting value, ending iteration if
	/// an inactive optional is returned. The returned range satisfies
	/// std::forward_range, and thus the function must be deterministic for
	/// any set of inputs as well as being copy-constructible.
	/// </summary>
	template<std::invocable Generator, class Start>
	requires detail::is_optional<std::invoke_result_t<Generator>>
		&& std::convertible_to<Start, detail::optional_unwrap_t<std::invoke_result_t<Generator>>>
	constexpr auto generate_multi(Start &&start, Generator &&g)
	{
		return detail::GenerateOptionalRangeWithStart<Generator, false>{ std::forward<Start>(start), std::move(g) };
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
				out.push_back(ranges::iter_move(it));
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
			return range | std::views::transform([i = (size_t)0](auto &&value) mutable
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
			Range(B &&begin, E &&end) : _begin(DIB_FWD(begin)), _end(DIB_FWD(end))
			{}

			auto begin() const { return _begin; }
			auto end() const { return _end; }
		};

		return Range(DIB_FWD(begin), DIB_FWD(end));
	}
}

#endif