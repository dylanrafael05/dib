#pragma once

#include <cmath>

#include "concept.h"
#include "raylib.h"

namespace dib::math
{   
    template<concepts::IsInteger T>
    constexpr T mod(T lhs, T rhs) { return lhs % rhs; }
    template<concepts::IsReal T>
    constexpr T mod(T lhs, T rhs) { return std::fmod(lhs, rhs); }

    template<concepts::IsLerpable T>
    constexpr T lerp(T start, T end, float t)
    {
        if(t == 0) return start;
        if(t == 1) return end;
        return (T)(t * (end - start)) + start;
    }

    template<> struct concepts::HasCustomLerp<Color> : std::true_type {};
    inline Color lerp(Color start, Color end, float t)
    {
        if (t == 0) return start;
        if (t == 1) return end;
        return Color
        {
            .r = lerp(start.r, end.r, t),
            .g = lerp(start.g, end.g, t),
            .b = lerp(start.b, end.b, t),
            .a = lerp(start.a, end.a, t),
        };
    }

    template<concepts::IsReal T>
    constexpr float invlerp(T value, T start, T end)
    {
        return static_cast<float>((value - start) / (end - start));
    } 

    template<concepts::IsArithmetic T>
    constexpr int sign(T value)
    {
        if(value > (T)0)
            return 1;
        
        if(value < (T)0)
            return -1;

        return 0;
    }

    template<concepts::IsArithmetic T>
    constexpr T max(T a, T b)
    {
        if(a > b) return a;
        return b;
    }

    template<concepts::IsArithmetic T>
    constexpr T min(T a, T b)
    {
        if(a < b) return a;
        return b;
    }

    template<concepts::IsArithmetic T> 
    constexpr T clamp(T a, T min = (T)0, T max = (T)1)
    {
        if(a < min) return min;
        if(a > max) return max;

        return a;
    }
    
    template<concepts::IsReal T>
    constexpr T fract(T value)
    {
        return value - (T)floor((double)value);
    }

    template<concepts::IsArithmetic T>
    constexpr T abs(T value)
    {
        return value > 0 ? value : -value;
    }
    
    template<concepts::IsReal T>
    constexpr T smoothstep(T start, T end, float value)
    {
        auto value2 = value * value;
        auto value3 = value2 * value;

        auto step = clamp(3*value2 - 2*value3);

        return start + step * (end - start);
    }

    extern template float lerp(float, float, float);
    extern template float invlerp(float, float, float);

    extern template int sign(float);
    extern template int sign(int);
    
    extern template float abs(float);
    extern template int abs(int);
    
    extern template int max(int, int);
    extern template unsigned int max(unsigned int, unsigned int);
    extern template float max(float, float);
    
    extern template int min(int, int);
    extern template unsigned int min(unsigned int, unsigned int);
    extern template float min(float, float);
    
    extern template int clamp(int, int, int);
    extern template unsigned int clamp(unsigned int, unsigned int, unsigned int);
    extern template float clamp(float, float, float);
    
    extern template float fract(float);
    extern template float smoothstep(float, float, float);

    namespace noise
    {
        float permute(float x);
        float randomize(float x);
    }
}