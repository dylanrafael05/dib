#include "dib/math/externs.h"
#include "dib/math/vec.h"
#include "dib/math/misc.h"

using namespace dib::math;
using uint = dib::math::uint;

// Class definitions //
#define __func(T, N, ...) \
    template class dib::math::vec<T, N>;

__DIBMATH_ALL_EXTERNS()
#undef __func

// Arithmetic operator definitions //
#define __func(T, N, ...)                                              \
    template vec<T, N> dib::math::operator+(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator+(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator+(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator+=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator+=(vec<T, N> &, T);         \
    template vec<T, N> dib::math::operator-(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator-(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator-(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator-=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator-=(vec<T, N> &, T);         \
    template vec<T, N> dib::math::operator*(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator*(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator*(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator*=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator*=(vec<T, N> &, T);         \
    template vec<T, N> dib::math::operator/(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator/(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator/(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator/=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator/=(vec<T, N> &, T);

__DIBMATH_SCALAR_EXTERNS()
#undef __func

// Modulo operator definitions //
#define __func(T, N, ...)                                              \
    template vec<T, N> dib::math::operator%(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator%(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator%(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator%=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator%=(vec<T, N> &, T);

__DIBMATH_INTEGRAL_EXTERNS()
#undef __func

// Boolean operator definitions //
#define __func(T, N, ...)                                              \
    template vec<T, N> dib::math::operator&(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator&(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator&(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator&=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator&=(vec<T, N> &, T);         \
    template vec<T, N> dib::math::operator|(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator|(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator|(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator|=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator|=(vec<T, N> &, T);         \
    template vec<T, N> dib::math::operator^(vec<T, N>, vec<T, N>);     \
    template vec<T, N> dib::math::operator^(vec<T, N>, T);             \
    template vec<T, N> dib::math::operator^(T, vec<T, N>);             \
    template vec<T, N> &dib::math::operator^=(vec<T, N> &, vec<T, N>); \
    template vec<T, N> &dib::math::operator^=(vec<T, N> &, T);

__DIBMATH_BITWISE_EXTERNS()
#undef __func

// Implementation for unary operators //
#define __func(T, N, ...)                               \
    template vec<T, N> dib::math::operator-(vec<T, N>); \
    template vec<T, N> dib::math::operator+(vec<T, N>);

__DIBMATH_SCALAR_EXTERNS()
#undef __func

#define __func(T, N, ...)                               \
    template vec<T, N> dib::math::operator!(vec<T, N>);

__DIBMATH_BOOL_EXTERNS()
#undef __func

#define __func(T, N, ...)                               \
    template vec<T, N> dib::math::operator~(vec<T, N>); \

__DIBMATH_INTEGRAL_EXTERNS()
#undef __func

// Miscelanneous functions //
float dib::math::noise::permute(float x)
{
    return fmodf((499 + 17 * x) * (31 * x + 1.0f / 23), 289);
}

float dib::math::noise::randomize(float x)
{
    return fract(permute(permute(permute(x))));
}

template float dib::math::lerp(float, float, float);
template float dib::math::invlerp(float, float, float);

template int dib::math::sign(float);
template int dib::math::sign(int);

template float dib::math::abs(float);
template int dib::math::abs(int);

template int dib::math::max(int, int);
template unsigned int dib::math::max(unsigned int, unsigned int);
template float dib::math::max(float, float);

template int dib::math::min(int, int);
template unsigned int dib::math::min(unsigned int, unsigned int);
template float dib::math::min(float, float);

template int dib::math::clamp(int, int, int);
template unsigned int dib::math::clamp(unsigned int, unsigned int, unsigned int);
template float dib::math::clamp(float, float, float);

template float dib::math::fract(float);
template float dib::math::smoothstep(float, float, float);