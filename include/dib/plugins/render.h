#pragma once

#include "dib/app.fwd.h"
#include "dib/ecs/systems_fwd.h"
#include "dib/ecs/entities.h"
#include "dib/ecs/singletons.h"
#include "dib/math/vec.h"

#include "raylib.h"

namespace dib::plugins
{
    struct RenderPlugin
    {
        Color backgroundClear = RAYWHITE;

        void inject(dib::App &app) const;
    };

    struct [[=ecs::singleton]] CameraHandler 
    {
        dib::ecs::EntityID main_camera;
    };

    enum class Camera3DProjection : int
    {
        Perspective = 0,
        Orthographic = 1,
    };
    
    struct [[=ecs::component]] CameraComponent2D
    {
        union
        {
            struct
            {
                math::float2 offset;
                math::float2 target;
                float rotation;
                float zoom;
            };

            Camera2D raylib;
        };
    };

    struct [[=ecs::component]] CameraComponent3D
    {
        union
        {
            struct 
            {
                math::float3 position;
                math::float3 target;
                math::float3 up;
                float fovy;

                Camera3DProjection projection;
            };
            
            Camera3D raylib;
        };
    };

    namespace groups
    {
        extern ecs::decl::SystemGroup RenderWorld;
        extern ecs::decl::SystemGroup RenderUI;
    }

    void clear_background();
}