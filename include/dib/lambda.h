#pragma once

#include "dib/preprocess.h"

/// A set of preprocessor macros which enable a shorthand syntax for lambdas.
/// Lambdas that take in N arguments can be spelled as DIB_LMB_N(_1 + _2 + _3 + ... _N)
/// for N between 0 and 4. N can also be left as 'n' to allow for a variadic lambda which
/// takes one pack parameter _args. DIB_OVERSET is also provided to create a lambda which
/// behaves as the overload set of the function it is provided.
///
/// If desired, one may define 'DIB_LMB_SHORT_SYNTAX' before including this file
/// to permit the usage of the shorthand λN and λv for lambdas and overload sets respectively.

#define __DIB_UNWRAP_1(...) __VA_ARGS__
#define __DIB_UNWRAP(...) __DIB_UNWRAP_1(__VA_ARGS__)
#define __DIB_LMB_BODY(...) \
	noexcept(requires { {__VA_ARGS__} noexcept; }) -> decltype(auto) requires requires { __VA_ARGS__; } { return __VA_ARGS__; }

#define DIB_LMB_0_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture]() noexcept(requires { {__VA_ARGS__} noexcept; }) -> decltype(auto) { return __VA_ARGS__; })
#define DIB_LMB_1_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&_1) __DIB_LMB_BODY(__VA_ARGS__))
#define DIB_LMB_2_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&_1, auto &&_2) __DIB_LMB_BODY(__VA_ARGS__))
#define DIB_LMB_3_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&_1, auto &&_2, auto &&_3) __DIB_LMB_BODY(__VA_ARGS__))
#define DIB_LMB_4_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&_1, auto &&_2, auto &&_3, auto &&_4) __DIB_LMB_BODY(__VA_ARGS__))
#define DIB_LMB_1n_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&_1, auto &&..._args) __DIB_LMB_BODY(__VA_ARGS__))
#define DIB_LMB_n_C(_capture, ...) \
	__DIB_UNWRAP([__DIB_UNWRAP _capture](auto &&..._args) __DIB_LMB_BODY(__VA_ARGS__))

#define DIB_LMB_0(...) DIB_LMB_0_C((&), __VA_ARGS__)
#define DIB_LMB_1(...) DIB_LMB_1_C((&), __VA_ARGS__)
#define DIB_LMB_2(...) DIB_LMB_2_C((&), __VA_ARGS__)
#define DIB_LMB_3(...) DIB_LMB_3_C((&), __VA_ARGS__)
#define DIB_LMB_4(...) DIB_LMB_4_C((&), __VA_ARGS__)
#define DIB_LMB_1n(...) DIB_LMB_1n_C((&), __VA_ARGS__)
#define DIB_LMB_n(...) DIB_LMB_n_C((&), __VA_ARGS__)
#define DIB_OVERSET(fn) DIB_LMB_n_C((&), fn(FORWARD(_args)...))

#ifdef DIB_LMB_SHORT_SYNTAX
#	define λ0(...)				DIB_LMB_0(__VA_ARGS__)
#	define λ1(...)				DIB_LMB_1(__VA_ARGS__)
#	define λ2(...)				DIB_LMB_2(__VA_ARGS__)
#	define λ3(...)				DIB_LMB_3(__VA_ARGS__)
#	define λ4(...)				DIB_LMB_4(__VA_ARGS__)
#	define λ1n(...)				DIB_LMB_1n(__VA_ARGS__)
#	define λn(...)				DIB_LMB_n(__VA_ARGS__)
#	define λ0c(_capture...)		DIB_LMB_0_C(_capture, __VA_ARGS__)
#	define λ1c(_capture...)		DIB_LMB_1_C(_capture, __VA_ARGS__)
#	define λ2c(_capture...)		DIB_LMB_2_C(_capture, __VA_ARGS__)
#	define λ3c(_capture...)		DIB_LMB_3_C(_capture, __VA_ARGS__)
#	define λ4c(_capture...)		DIB_LMB_4_C(_capture, __VA_ARGS__)
#	define λnc(_capture...)		DIB_LMB_n_C(_capture, __VA_ARGS__)
#	define λ1nc(_capture...)	DIB_LMB_1n_C(_capture, __VA_ARGS__)
#	define λv(fn)				DIB_OVERSET(fn)
#endif