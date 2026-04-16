#pragma once

#include "dib/resources/resources.h"
#include "dib/option.h"
#include "raylib.h"

namespace dib::res
{
    class Shader2D 
    {
        dib::option::Option<ResourceHandle<Text>> _frag;
        dib::option::Option<ResourceHandle<Text>> _vert;
        std::vector<ResourceHandle<Text>> _includes;

        ::Shader _shader;

        friend struct ResourceInterface<Shader2D>;

    public:
        ::Shader get() const;
    };

    template<> struct ResourceInterface<Shader2D>
    {
        static void load(Resources &resources, Shader2D &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, Shader2D &instance);
        static void get_dependencies(Shader2D &instance, std::vector<std::string> &deps);

        static constexpr bool open_as_text = true;
        static constexpr bool free_underlying_once_loaded = true;
    };
}