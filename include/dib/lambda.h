#ifndef __DIB_LAMBDA_H
#define __DIB_LAMBDA_H

#include "dib/preprocess.h"
#include <string_view>

namespace dib::lambda
{
	struct FakeArgument
	{
		constexpr FakeArgument() = default;
		FakeArgument(const FakeArgument &) = delete;
		FakeArgument(FakeArgument &&) = delete;
	};

	constexpr bool expr_border_char(char c)
	{
		return '0' <= c && c <= '9'
			|| 'a' <= c && c <= 'z'
			|| 'A' <= c && c <= 'Z'
			|| c == '"';
	}

	constexpr bool is_digit_char(char c)
	{
		return '0' <= c && c <= '9';
	}

	constexpr inline int var_expr = -1;
	constexpr inline int expr_arg_count(std::string_view text)
	{
		int result = 0;

		for (size_t i = 0; i < text.size(); i++)
		{
			auto border = [&](size_t s)
				{
					return (i == 0 || !expr_border_char(text[i - 1]))
						&& (i + s >= text.size() || !expr_border_char(text[i + s]));
				};

			auto check = [&](std::string_view s)
				{
					return text.substr(i).starts_with(s) && border(s.size());
				};


			if (text[i] == '"' && (i == 0 || !is_digit_char(text[i-1])))
			{
				i++;
				while (text[i] != '"')
				{
					if (text[i] == '\\') i++;
					i++;
				}
			}
			else if (text[i] == '\'')
			{
				i++;
				while (text[i] != '\'')
				{
					if (text[i] == '\\') i++;
					i++;
				}
			}
			else if (check("_1"))
			{
				result = std::max(1, result);
			}
			else if (check("_2"))
			{
				result = std::max(2, result);
			}
			else if (check("_3"))
			{
				result = std::max(3, result);
			}
			else if (check("_4"))
			{
				result = std::max(4, result);
			}
			else if (check("_args"))
			{
				result = var_expr;
			}
		}

		return result;
	}
}

#define __DIB_STRINGIFY(...) #__VA_ARGS__
#define __DIB_UNWRAP(...) __VA_ARGS__

#define __DIB_LP (
#define __DIB_RP )
#define __DIB_CALL(macro, ...) __DIB_UNWRAP(macro __DIB_LP __VA_ARGS__ __DIB_RP)

#define __DIB_LMB_FA ::dib::lambda::FakeArgument

#define __DIB_ARITY(...) \
	::dib::lambda::expr_arg_count(__DIB_CALL(__DIB_STRINGIFY, __VA_ARGS__))

#define __DIB_LMB_BODY(...) \
	noexcept(requires { {__VA_ARGS__} noexcept; }) -> decltype(auto) requires requires { __VA_ARGS__; } { return __VA_ARGS__; }

#define DIB_LMB_NC_0(_capture, ...) \
	[__DIB_UNWRAP _capture]() noexcept(requires { {__VA_ARGS__} noexcept; }) -> decltype(auto) { return __VA_ARGS__; }
#define DIB_LMB_NC_1(_capture, ...) \
	[__DIB_UNWRAP _capture](auto &&_1) __DIB_LMB_BODY(__VA_ARGS__)
#define DIB_LMB_NC_2(_capture, ...) \
	[__DIB_UNWRAP _capture](auto &&_1, auto &&_2) __DIB_LMB_BODY(__VA_ARGS__)
#define DIB_LMB_NC_3(_capture, ...) \
	[__DIB_UNWRAP _capture](auto &&_1, auto &&_2, auto &&_3) __DIB_LMB_BODY(__VA_ARGS__)
#define DIB_LMB_NC_4(_capture, ...) \
	[__DIB_UNWRAP _capture](auto &&_1, auto &&_2, auto &&_3, auto &&_4) __DIB_LMB_BODY(__VA_ARGS__)
#define DIB_LMB_NC_n(_capture, ...) \
	[__DIB_UNWRAP _capture](auto &&..._args) __DIB_LMB_BODY(__VA_ARGS__)

#define DIB_LMB_0(...) DIB_LMB_NC_0((&), __VA_ARGS__)
#define DIB_LMB_1(...) DIB_LMB_NC_1((&), __VA_ARGS__)
#define DIB_LMB_2(...) DIB_LMB_NC_2((&), __VA_ARGS__)
#define DIB_LMB_3(...) DIB_LMB_NC_3((&), __VA_ARGS__)
#define DIB_LMB_4(...) DIB_LMB_NC_4((&), __VA_ARGS__)
#define DIB_LMB_n(...) DIB_LMB_NC_n((&), __VA_ARGS__)

#define DIB_LMB_NC(_capture, ...) \
	[&]([[maybe_unused]] const __DIB_LMB_FA &_1, \
		[[maybe_unused]] const __DIB_LMB_FA &_2, \
		[[maybe_unused]] const __DIB_LMB_FA &_3, \
		[[maybe_unused]] const __DIB_LMB_FA &_4, \
		[[maybe_unused]] auto &&..._args) { \
		constexpr auto _arity = __DIB_ARITY(__VA_ARGS__); \
		if constexpr(_arity == 0) return DIB_LMB_NC_0(_capture, __VA_ARGS__); \
		else if constexpr(_arity == 1) return DIB_LMB_NC_1(_capture, __VA_ARGS__); \
		else if constexpr(_arity == 2) return DIB_LMB_NC_2(_capture, __VA_ARGS__); \
		else if constexpr(_arity == 3) return DIB_LMB_NC_3(_capture, __VA_ARGS__); \
		else if constexpr(_arity == 4) return DIB_LMB_NC_4(_capture, __VA_ARGS__); \
		else if constexpr(_arity == -1) return DIB_LMB_NC_n(_capture, __VA_ARGS__); \
		else return; \
	}(__DIB_LMB_FA{}, __DIB_LMB_FA{}, __DIB_LMB_FA{}, __DIB_LMB_FA{})

#define DIB_LMB(...) DIB_LMB_NC((&), __VA_ARGS__)
#define DIB_OVERSET(fn) DIB_LMB_n(fn(DIB_FWD(_args)...))

#ifdef DIB_LMB_SHORT_SYNTAX
#	define λc(capture, ...)	DIB_LMB_NC(capture, __VA_ARGS__)
#	define λ(...)			DIB_LMB(__VA_ARGS__)
#	define λ0(...)			DIB_LMB_0(__VA_ARGS__)
#	define λ1(...)			DIB_LMB_1(__VA_ARGS__)
#	define λ2(...)			DIB_LMB_2(__VA_ARGS__)
#	define λ3(...)			DIB_LMB_3(__VA_ARGS__)
#	define λ4(...)			DIB_LMB_4(__VA_ARGS__)
#	define λn(...)			DIB_LMB_n(__VA_ARGS__)
#	define λv(fn)			DIB_OVERSET(fn)
#endif

#endif