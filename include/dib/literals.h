#pragma once

#include <stdint.h>
#include <stddef.h>

#include "dib/debug.h"
#include "raylib.h"

namespace dib::literals
{
    class BadColorLiteral {};

    namespace detail
    {
        constexpr uint8_t hex_val(char x)
        {
            if('A' <= x && x <= 'F') return x - 'A' + 10;
            if('a' <= x && x <= 'f') return x - 'a' + 10;
            return x - '0';
        }
        constexpr uint8_t hex_lit(char a, char b)
        {
            return 16 * hex_val(a) + hex_val(b);
        }
    }

    constexpr Color hex_color(const char *str, size_t size)
    {
        if(size != 9 && size != 7) RUNTIME_ERROR("Bad color literal. Must be 9 or 7 characters long.");
        if(str[0] != '#') RUNTIME_ERROR("Bad color literal. Must start with a hashtag.");

        Color out;

        out.r = detail::hex_lit(str[1], str[2]);
        out.g = detail::hex_lit(str[3], str[4]);
        out.b = detail::hex_lit(str[5], str[6]);
        
        if(size == 7)
        {
            out.a = 255;
        }
        else
        {
            out.a = detail::hex_lit(str[7], str[8]);
        }

        return out;
    }

    namespace
    {
        constexpr Color operator""_col(const char *str, size_t size)
        {
            return hex_color(str, size);
        }
    }
}