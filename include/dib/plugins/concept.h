#ifndef __DIBAPP_PLUGINS_CONCEPTS_H
#define __DIBAPP_PLUGINS_CONCEPTS_H

#include "../app.fwd.h"
#include "dib/types.h"

namespace dib::plugins
{
    template<class T>
    concept IsPlugin = requires(dib::App &app, const T &value)
    {
        {value.inject(app)} -> types::IsVoid;
        std::is_default_constructible_v<T>;
    };
}

#endif