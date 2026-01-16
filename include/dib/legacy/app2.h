#ifndef __DIBAPP2_USER_H
#define __DIBAPP2_USER_H

// TODO: nuke app2 and steal the best parts of it for app

#include "app2.lib.h"

inline void dib::app2::App::run()
{
    run(is_static);
}

inline dib::resources::Resources &dib::app2::resource_manager()
{
    return detail::resource_manager(use_batch);
}

#endif