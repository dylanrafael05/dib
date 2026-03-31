#include "dib/ecs/systems_fwd.h"

namespace dib::ecs::groups
{
    decl::SystemGroup Main;

    decl::SystemGroup First;
    decl::SystemGroup Last;

    decl::SystemGroup PreUpdate;
    decl::SystemGroup Update;
    decl::SystemGroup PostUpdate;

    decl::SystemGroup StartRender;
    decl::SystemGroup Render;
    decl::SystemGroup EndRender;

    decl::SystemGroup OnInit;
    decl::SystemGroup OnDeinit;

    decl::SystemGroup Start;

    decl::SystemGroup HandleStateTransitions;
}