#ifndef __DIBAPP_PLUGINS_AUDIO_H
#define __DIBAPP_PLUGINS_AUDIO_H

#include "dib/app.fwd.h"

namespace dib::plugins
{
    struct AudioPlugin
    {
        void inject(dib::App &app) const;
    };
}

#endif