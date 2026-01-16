#pragma once

#include <concepts>

namespace dib::math::concepts
{
    template<class T> concept IsInteger = std::is_integral_v<T>;
    template<class T> concept IsReal = std::is_floating_point_v<T>;
    template<class T> concept IsScalar = std::is_scalar_v<T>;
    template<class T> concept IsArithmetic = std::is_arithmetic_v<T>;

    template<class T> struct HasCustomLerp : public std::false_type {};
    template<class T> constexpr bool has_custom_lerp = HasCustomLerp<T>::value;

    template<class T> concept IsLerpable = 
        requires(T a, T b, float f)
        {
            {a + b} -> std::convertible_to<std::remove_cvref_t<T>>;
            {a - b} -> std::convertible_to<std::remove_cvref_t<T>>;
            (std::remove_cvref_t<T>)(a * f);
        } 
        || has_custom_lerp<std::remove_cvref_t<T>>;
}