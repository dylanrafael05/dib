#ifndef __DIBAPP_PLUGIN_POSITION_H
#define __DIBAPP_PLUGIN_POSITION_H

#include "../math/vec.h"

namespace dib::plugins
{
    struct Position2D
    {
        dib::math::float2 value;

        constexpr Position2D(float x, float y)
            : value(x, y)
        {}
        constexpr Position2D(dib::math::float2 xy)
            : value(xy)
        {}
        constexpr Position2D() {}
    };

    struct Position3D
    {
        dib::math::float3 value;

        constexpr Position3D(float x, float y, float z)
            : value(x, y, z)
        {}
        constexpr Position3D(dib::math::float3 xyz)
            : value(xyz)
        {}
        constexpr Position3D() {}
    };
}

#endif