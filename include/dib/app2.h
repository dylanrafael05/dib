#ifndef __DIBAPP2_USER_H
#define __DIBAPP2_USER_H

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