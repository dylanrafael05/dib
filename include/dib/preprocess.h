#pragma once

#define FORWARD(...) static_cast<decltype(__VA_ARGS__)>(__VA_ARGS__)
#define MOVE(...) static_cast<::std::remove_reference_t<decltype(__VA_ARGS__)> &&>(__VA_ARGS__)

#if defined(__GCC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER)
#define DIBCOMPILER_gcc
#endif

#if defined(__clang__)
#define DIBCOMPILER_clang
#endif