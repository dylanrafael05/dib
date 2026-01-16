#pragma once

#include <stddef.h>

#include "dib/math/vec.h"
#include "dib/types.h"

namespace dib::math
{
    struct quaternion : types::TriviallyRelocatable
    {
        float i;
        float j;
        float k;
        float real;

        // CONSTRUCTORS //
        quaternion()
            : i(0), j(0), k(0), real(1)
        {}
        quaternion(float real, float i, float j, float k)
            : i(i), j(j), k(k), real(real)
        {}
        quaternion(Quaternion quat)
            : i(quat.x), j(quat.y), k(quat.z), real(quat.w)
        {}

        operator Quaternion() const;

        // TUPLE PROTOCOL //
        template<size_t I> float &get()
        {
            static_assert(I <= 3, "Invalid quaternion index.");  

            if constexpr(I == 0) return real;
            else if constexpr(I == 1) return i;
            else if constexpr(I == 2) return j;
            else if constexpr(I == 3) return k;
        }
        
        template<size_t I> const float &get() const
        {
            static_assert(I <= 3, "Invalid quaternion index.");   

            if constexpr(I == 0) return real;
            else if constexpr(I == 1) return i;
            else if constexpr(I == 2) return j;
            else if constexpr(I == 3) return k;
        }
    };

    bool operator==(quaternion lhs, quaternion rhs);

    /// @brief Multiply two quaternions, combining them into one.
    quaternion operator*(quaternion lhs, quaternion rhs);
    /// @brief Apply a quaternion rotation to the provided position.
    float3 operator*(quaternion lhs, float3 rhs);

    /// @brief Create a unit quaternion which rotates by the provided
    /// euler angles.
    quaternion euler_angles(float3 angles);
    /// @brief Calculate the euler angles from the provided unit quaternion.
    float3 get_euler_angles(quaternion quat);

    /// @brief Create a quaternion which rotates around the given axis
    /// by a given amount in degrees.
    quaternion axis_angle(float3 axis, float angle);
    /// @brief Calculate the angle which this quaternion rotates by in degrees.
    float get_angle(quaternion quat);
    /// @brief Calculate the unit axis which this quaternion rotates.
    float3 get_axis(quaternion quat);

    /// @brief Invert a unit quaternion such that it undoes the original
    /// quaternion's rotation.
    quaternion invert(quaternion quat);
}

namespace std
{
    template<> struct tuple_size<dib::math::quaternion> {constexpr static size_t value = 4;};
    template<size_t I> struct tuple_element<I, dib::math::quaternion> {using type = float;};
}