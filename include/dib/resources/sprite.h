#pragma once

#include "dib/resources/resources.h"
#include "raylib.h"

namespace dib::res
{
    struct [[=json::derive, =res::json_resource]] Sprite
    {
        res::ResourceHandle<Texture2D> texture;
        Rectangle rect;
    };
}