#ifndef __DIBAPP_PLUGINS_CAMERA_H
#define __DIBAPP_PLUGINS_CAMERA_H

#include "../app.fwd.h"
#include "../ecs/systems_fwd.h"
#include "../ecs/singletons.h"
#include "../ecs/components.h"

#include "raylib.h"

namespace dib::plugins
{
    struct RenderPlugin
    {
        Color backgroundClear = RAYWHITE;

        void inject(dib::app::App &app) const;
    };

    struct CameraHandler 
    {
        dib::ecs::EntityID main_camera;
    };
    
    struct Camera2DT
    {
        ::Camera2D value;
    };
    struct Camera3DT
    {
        ::Camera3D value;
    };

    namespace groups
    {
        inline ecs::decl::SystemGroup RenderWorld;
        inline ecs::decl::SystemGroup RenderUI;
    }

    void clear_background(ecs::Singletons &);
}

#endif