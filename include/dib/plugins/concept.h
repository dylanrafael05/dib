#ifndef __DIBAPP_PLUGINS_CONCEPTS_H
#define __DIBAPP_PLUGINS_CONCEPTS_H

#include "../app.fwd.h"
#include <concepts>

namespace dib::plugins
{
    template<class T>
    concept plugin = requires(dib::app::App &app, const T &value)
    {
        {value.inject(app)} -> std::same_as<void>;
        std::is_default_constructible_v<T>;
    };
}

#endif