#ifndef __DIBAPP_PLUGINS_AUDIO_H
#define __DIBAPP_PLUGINS_AUDIO_H

#include "../app.fwd.h"

namespace dib::plugins
{
    struct AudioPlugin
    {
        void inject(dib::app::App &app) const;
    };
}

#endif