#include "dib/math/quaternion.h"
#include <bit>

using namespace dib::math;

// OPERATORS //
quaternion::operator Quaternion() const
{
    return std::bit_cast<Quaternion>(*this);
}

bool dib::math::operator==(quaternion lhs, quaternion rhs)
{
    return lhs.real == rhs.real
        && lhs.i == rhs.i
        && lhs.j == rhs.j
        && lhs.k == rhs.k;
}

quaternion dib::math::operator*(quaternion lhs, quaternion rhs)
{
    auto [a1, b1, c1, d1] = lhs;
    auto [a2, b2, c2, d2] = rhs;

    return {
        a1*a2 - b1*b2 - c1*c2 - d1*d2,
        a1*b2 + b1*a2 + c1*d2 - d1*c2,
        a1*c2 - b1*d2 + c1*a2 + d1*b2,
        a1*d2 + b1*c2 - c1*b2 + d1*a2
    };
}

float3 dib::math::operator*(quaternion lhs, float3 rhs)
{
    auto inv = dib::math::invert(lhs);
    auto rhs_q = quaternion{0, rhs.x, rhs.y, rhs.z};
    auto out = lhs * rhs_q * inv;

    return {out.i, out.j, out.k};
}

// HELPER CONSTRUCTORS //
quaternion dib::math::euler_angles(float3 angles)
{
    auto [u, v, w] = angles * (RAD2DEG / 2);

    auto cu = cosf(u);
    auto cv = cosf(v);
    auto cw = cosf(w);

    auto su = sinf(u);
    auto sv = sinf(v);
    auto sw = sinf(w);

    return {
        cu*cv*cw + su*sv*sw,
        su*cv*cw - cu*sv*sw,
        cu*sv*cw + su*cv*sw,
        cu*cv*sw - su*sv*cw
    };
}

float3 dib::math::get_euler_angles(quaternion quat)
{
    auto [q0, q1, q2, q3] = quat;

    auto pitch = asinf(2 * (q0*q2 - q1*q3));

    if(pitch == PI / 2)
    {
        return float3{pitch, 0, -2*atan2f(q1, q0)} * DEG2RAD;
    }
    else if(pitch == -PI / 2)
    {
        return float3{pitch, 0, +2*atan2f(q1, q0)} * DEG2RAD;
    }

    return float3{
        atan2f(2*(q0*q1 + q2*q3), q0*q0 - q1*q1 - q2*q2 - q3*q3),
        pitch,
        atan2f(2*(q0*q3 + q1*q2), q0*q0 + q1*q1 - q2*q2 - q3*q3)
    } * DEG2RAD;
}

quaternion dib::math::axis_angle(float3 axis, float angle)
{
    math::normalize_inplace(axis);

    float s = sinf(angle / 2 * DEG2RAD);
    float c = cosf(angle / 2 * DEG2RAD);

    return {c, s * axis.x, s * axis.y, s * axis.z};
}

float dib::math::get_angle(quaternion quat)
{
    return acosf(quat.real) * 2 * RAD2DEG;
}

float3 dib::math::get_axis(quaternion quat)
{
    auto sin = sqrtf(1 - quat.real*quat.real);
    return float3{quat.i, quat.j, quat.k} / sin;
}

// QUATERNION SPECIFIC OPERATIONS //
quaternion dib::math::invert(quaternion quat)
{
    return {
        quat.real,
        -quat.i,
        -quat.j,
        -quat.k
    };
}