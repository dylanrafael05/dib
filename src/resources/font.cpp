#include "dib/resources/font.h"
#include "dib/raylib.h"

#include <cmath>

using namespace dib;
using namespace dib::res;
using rFont = dib::res::Font;

// res::Font members //
u32 rFont::get_level_from_size(float size)
{
    return (u32)std::ceil(size * 5);
    // return (u32) math::max(std::log2(size), 0.f);
}

i32 rFont::get_size_from_level(u32 level)
{
    return (i32)level;
    // return std::pow(2, level);
}

::Font rFont::get_for_level(i32 level) const
{
    if(auto it = _loaded_fonts.find(level); it != _loaded_fonts.end())
    {
        return it->second;
    }

    auto [it, _] = _loaded_fonts.insert({ level, ::LoadFontFromMemory(
        _file_ext.c_str(), 
        (unsigned char *)_raw_font->data(), 
        _raw_font->size(), 
        get_size_from_level(level), 
        NULL, 0) });

    SetTextureFilter(it->second.texture, TEXTURE_FILTER_BILINEAR);
    
    return it->second;
}

::Font rFont::get(float size) const
{
    if(size < 0)
        return get_for_level(get_level_from_size(DefaultSize));

    return get_for_level(get_level_from_size(size * dib::raylib::get_zoom_2d()));
}

// ResourceInterface<res::Font> //
void ResourceInterface<rFont>::load(Resources &, rFont &instance, std::string_view, const char *buffer, size_t)
{
    std::stringstream buffer_stream(buffer);
    json::JsonReader json_read(buffer_stream);

    rFont::JsonProxy prox;
    json_read.read(prox);

    instance._raw_font = prox.font_file;
    instance._file_ext = std::filesystem::path(prox.font_file.name()).extension().string();
}

void ResourceInterface<rFont>::unload(Resources &, rFont &instance)
{
    for(auto [_, font] : instance._loaded_fonts)
    {
        UnloadFont(font);
    }
}