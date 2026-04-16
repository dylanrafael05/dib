#pragma once

#include <type_traits> // IWYU pragma: keep

#define FORWARD(...) static_cast<decltype(__VA_ARGS__)>(__VA_ARGS__)
#define MOVE(...) static_cast<::std::remove_reference_t<decltype(__VA_ARGS__)> &&>(__VA_ARGS__)

#if defined(__GCC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER)
#define DIBCOMPILER_gcc
#endif

#if defined(__clang__)
#define DIBCOMPILER_clang
#endif

#define __PPS_lprn (
#define __PPS_rprn )
#define __PPS_str(...) #__VA_ARGS__
#define __PPS_eval2(x) x
#define __PPS_eval(x) __PPS_eval2(x)

#define IS_FLAG_DEFINED(...) ( \
    sizeof(__PPS_eval(__PPS_str __PPS_lprn __VA_ARGS__ __PPS_rprn)) == 1)