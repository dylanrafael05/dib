#pragma once

#include "dib/ints.h"
#include "dib/resources/resources.h"
#include "dib/math/misc.h"
#include "raylib.h"
#include <unordered_map>

namespace dib::res
{
    struct Font
    {
    private:
        struct [[=json::derive]] JsonProxy
        {
            res::ResourceHandle<res::Buffer> font_file;
        };

        static constexpr float DefaultSize = 24;

        std::string _file_ext;
        res::ResourceHandle<res::Buffer> _raw_font;
        mutable std::unordered_map<u32, ::Font> _loaded_fonts;

        static u32 get_level_from_size(float size);
        static i32 get_size_from_level(u32 level);

        ::Font get_for_level(i32 level) const;

        friend struct ResourceInterface<Font>;

    public:
        ::Font get(float size = -1) const;
    };

    template<>
    struct ResourceInterface<Font>
    {
        static void load(Resources &resources, Font &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, Font &instance);

        static constexpr bool open_as_text = true;
        static constexpr bool free_underlying_once_loaded = true;
    };
}