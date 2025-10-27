#ifndef __DIBMATH_CONCEPT_H
#define __DIBMATH_CONCEPT_H

#include <concepts>

namespace dib::math::concepts
{
    template<class T> concept integer = std::is_integral_v<T>;
    template<class T> concept real = std::is_floating_point_v<T>;
    template<class T> concept scalar = std::is_scalar_v<T>;
    template<class T> concept arithmetic = std::is_arithmetic_v<T>;

    template<class T> struct HasCustomLerp : public std::false_type {};
    template<class T> constexpr bool has_custom_lerp = HasCustomLerp<T>::value;

    template<class T> concept lerpable = 
        requires(T a, T b, float f)
        {
            {a + b} -> std::convertible_to<std::remove_cvref_t<T>>;
            {a - b} -> std::convertible_to<std::remove_cvref_t<T>>;
            (std::remove_cvref_t<T>)(a * f);
        } 
        || has_custom_lerp<std::remove_cvref_t<T>>;
}

#endif