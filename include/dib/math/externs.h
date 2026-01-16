#pragma once

#define __DIBMATH_INTEGRAL_EXTERNS(...) \
    __func(int, 2, __VA_ARGS__) __func(int, 3, __VA_ARGS__) __func(int, 4, __VA_ARGS__) \
    __func(uint, 2, __VA_ARGS__) __func(uint, 3, __VA_ARGS__) __func(uint, 4, __VA_ARGS__)

#define __DIBMATH_REAL_EXTERNS(...) \
    __func(float, 2, __VA_ARGS__) __func(float, 3, __VA_ARGS__) __func(float, 4, __VA_ARGS__)

#define __DIBMATH_BOOL_EXTERNS(...) \
    __func(bool, 2, __VA_ARGS__) __func(bool, 3, __VA_ARGS__) __func(bool, 4, __VA_ARGS__)

#define __DIBMATH_SCALAR_EXTERNS(...) \
    __DIBMATH_INTEGRAL_EXTERNS(__VA_ARGS__) __DIBMATH_REAL_EXTERNS(__VA_ARGS__)

#define __DIBMATH_BITWISE_EXTERNS(...) \
    __DIBMATH_INTEGRAL_EXTERNS(__VA_ARGS__) __DIBMATH_BOOL_EXTERNS(__VA_ARGS__)

#define __DIBMATH_ALL_EXTERNS(...) \
    __DIBMATH_SCALAR_EXTERNS(__VA_ARGS__) __DIBMATH_BOOL_EXTERNS(__VA_ARGS__)
