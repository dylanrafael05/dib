#include "dib/app.h"
#include "dib/ecs/world.h"
#include "dib/ecs/systems.h"
#include "raylib.h"

using namespace dib;
using namespace dib::ecs;
using namespace dib::app;

void flush_commands()
{
    this_app().commands().flush();
}

void run_state_transitions()
{
    this_app().state_machine().update(
        this_app().systems());
}

void ecs::detail::initialize_main(Systems &scheduler)
{
    scheduler.add({

        System(groups::Main, groups::First),

        System(groups::Main, groups::PreUpdate, options::after(groups::First)),
        System(groups::Main, groups::Update, options::after(groups::PreUpdate)),
        System(groups::Main, groups::PostUpdate, options::after(groups::Update) | options::deinit_with(flush_commands)),

        System(groups::Main, groups::HandleStateTransitions, options::after(groups::PostUpdate)),
            System(groups::HandleStateTransitions, run_state_transitions),

        System(groups::Main, groups::StartRender, options::after(groups::HandleStateTransitions) | options::init_with(BeginDrawing)),
        System(groups::Main, groups::Render, options::after(groups::StartRender)),
        System(groups::Main, groups::EndRender, options::after(groups::Render) | options::deinit_with(EndDrawing)),

        System(groups::Main, groups::Last, options::after(groups::EndRender)),

    });
}