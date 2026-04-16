#include "dib/debug.h"
#include "dib/plugins/audio.h"
#include "dib/plugins/render.h"
#include "dib/ecs/systems.h"
#include "dib/app.h"

using namespace dib;
using namespace dib::plugins;
using namespace dib::ecs;

namespace gr = dib::ecs::groups;

namespace dib::plugins::groups
{
    ecs::decl::SystemGroup RenderWorld;
    ecs::decl::SystemGroup RenderUI;
}

struct [[=ecs::singleton]] RenderSettings
{
    Color bg_color;
};

void AudioPlugin::inject(App &app) const
{
    app.systems().add({
        System(gr::OnInit,   InitAudioDevice),
        System(gr::OnDeinit, CloseAudioDevice)
    });
}

void init_render()
{
    singletons().create<CameraHandler>();
}

void plugins::clear_background()
{
    ClearBackground(
        get_singleton<RenderSettings>().bg_color);
}

void begin_draw_world()
{
    auto &handler = get_singleton<CameraHandler>();
    auto cam = handler.main_camera;

    if(!cam.is_invalid())
    {
        if(cam.has_component<CameraComponent2D>())
        {
            BeginMode2D(cam.get_component<CameraComponent2D>().raylib);
        }
        else if(cam.has_component<CameraComponent3D>())
        {
            BeginMode3D(cam.get_component<CameraComponent3D>().raylib);
        }
        else
        {
            RUNTIME_ERROR("The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component.");
        }
    }
    else
    {
        // TODO: allow the user to specify a predicate which, when matched, prevents this check.
        RUNTIME_ERROR("There must always be an active main camera!");
    }
}

void end_draw_world()
{
    auto &handler = get_singleton<CameraHandler>();
    auto cam = handler.main_camera;

    if(!cam.is_invalid())
    {
        if(cam.has_component<CameraComponent2D>())
        {
            EndMode2D();
        }
        else if(cam.has_component<CameraComponent3D>())
        {
            EndMode3D();
        }
        else
        {
            RUNTIME_ERROR("The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component.");
        }
    }
    else
    {
        // TODO: allow the user to specify a predicate which, when matched, prevents this check.
        RUNTIME_ERROR("There must always be an active main camera!");
    }
}

void RenderPlugin::inject(App &app) const
{
    app.systems().add({
        System(gr::OnInit, init_render),
        System(gr::Render, clear_background),
        System(gr::Render, plugins::groups::RenderWorld, options::init_with(begin_draw_world) | options::deinit_with(end_draw_world) | options::after(clear_background)),
        System(gr::Render, plugins::groups::RenderUI, options::after(plugins::groups::RenderWorld))
    });

    app.singletons()
        .create<RenderSettings>(RenderSettings{.bg_color = backgroundClear});
}