#pragma once

#include "meta"
#include <string_view>

namespace dib::json
{
    struct DeriveJson {};
    constexpr DeriveJson derive;
    
    struct Rename { const char *to; };
    consteval Rename rename(std::string_view to)
    {
        return Rename { .to = std::define_static_string(to) };
    }
}