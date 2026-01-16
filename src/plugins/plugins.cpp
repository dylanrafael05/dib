#include "dib/debug.h"
#include "dib/plugins/audio.h"
#include "dib/plugins/render.h"
#include "dib/ecs/systems.h"
#include "dib/app.h"

using namespace dib;
using namespace dib::plugins;
using namespace dib::app;
using namespace dib::ecs;

namespace gr = dib::ecs::groups;

struct RenderSettings
{
    Color bg_color;
};

void AudioPlugin::inject(App &app) const
{
    app.systems().add({
        system(gr::OnInit,   InitAudioDevice),
        system(gr::OnDeinit, CloseAudioDevice)
    });
}

void init_render()
{
    this_app().singletons().create<CameraHandler>();
}

void plugins::clear_background()
{
    ClearBackground(
        this_app().singletons().get<RenderSettings>().bg_color);
}

void begin_draw_world()
{
    auto &world = this_app().world();
    auto &handler = this_app().singletons().get<CameraHandler>();
    auto cam = handler.main_camera;

    if(!cam.is_invalid())
    {
        if(world.has_component<Camera2DT>(cam))
        {
            BeginMode2D(world.get_component<Camera2DT>(cam).value);
        }
        else if(world.has_component<Camera3DT>(cam))
        {
            BeginMode3D(world.get_component<Camera3DT>(cam).value);
        }
        else
        {
            RUNTIME_ERROR("The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component.");
        }
    }
}

void end_draw_world()
{
    auto &world = this_app().world();
    auto &handler = this_app().singletons().get<CameraHandler>();
    auto cam = handler.main_camera;

    if(!cam.is_invalid())
    {
        if(world.has_component<Camera2DT>(cam))
        {
            EndMode2D();
        }
        else if(world.has_component<Camera3DT>(cam))
        {
            EndMode3D();
        }
        else
        {
            RUNTIME_ERROR("The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component.");
        }
    }
}

void RenderPlugin::inject(App &app) const
{
    app.systems().add({
        system(gr::OnInit, init_render),
        system(gr::Render, clear_background),
        system(gr::Render, plugins::groups::RenderWorld, options::init_with(begin_draw_world) | options::deinit_with(end_draw_world) | options::after(clear_background)),
        system(gr::Render, plugins::groups::RenderUI, options::after(plugins::groups::RenderWorld))
    });

    app.singletons()
        .create<RenderSettings>(RenderSettings{.bg_color = backgroundClear});
}