#include "dib/plugins/audio.h"
#include "dib/plugins/render.h"
#include "dib/ecs/systems.h"
#include "dib/app.h"

using namespace dib;
using namespace dib::plugins;
using namespace dib::app;
using namespace dib::ecs;

namespace gr = dib::ecs::groups;

struct RenderData
{
    Color bg_color;
};

void AudioPlugin::inject(App &app) const
{
    app.systems().add({
        System(gr::OnInit, InitAudioDevice),
        System(gr::OnDeinit, CloseAudioDevice)
    });
}

void init_render(Singletons &singletons)
{
    singletons.create<CameraHandler>();
}

void plugins::clear_background(Singletons &singletons)
{
    ClearBackground(singletons.get<RenderData>().bg_color);
}

void begin_draw_world(World &world, CameraHandler &handler)
{
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
            std::cerr << "The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component." << std::endl;
            std::abort();
        }
    }
}

void end_draw_world(World &world, CameraHandler &handler)
{
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
            std::cerr << "The main camera provided to dib::plugins::CameraHandler must have either a 2d or 3d camera component." << std::endl;
            std::abort();
        }
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

    app.world().singletons()
        .create<RenderData>(RenderData{.bg_color = backgroundClear});
}