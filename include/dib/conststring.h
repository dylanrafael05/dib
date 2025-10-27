#ifndef __DIB_CONSTSTRINGS_H
#define __DIB_CONSTSTRINGS_H

#include <stddef.h>
#include <string>
#include <string_view>

namespace dib::strings
{
    class string_literal
    {
        const char *ptr;
        size_t length;

    public:
        consteval string_literal(const char *str)
        {
            ptr = str;
            length = 0;
            
            for(const char *it = ptr; *it != '\0'; it++)
                length++;
        }

        constexpr string_literal()
            : ptr(""), length(0)
        {}

        constexpr const char *c_str() const {return ptr;}
        constexpr size_t size() const {return length;}

        constexpr operator std::string_view() const
        {
            return {c_str(), size()};
        }

        operator std::string() const
        {
            return {c_str(), size()};
        }

        constexpr auto begin() const { return c_str(); }
        constexpr auto end() const { return c_str() + size(); }
    };

    bool operator==(string_literal, const char *);
    bool operator==(const char *, string_literal);
    
    template<size_t N>
    struct string_const
    {
        char text[N];
        constexpr string_const(const char (&text)[N])
        {
            for(size_t i = 0; i < N; i++)
                this->text[i] = text[i];
        }

        consteval const char *c_str() const { return text; }
        consteval size_t size() const { return N - 1; }

        consteval operator std::string_view() const
        {
            return { c_str(), size() };
        }

        consteval auto begin() const { return text; }
        consteval auto end() const { return text + N; }
    };

    template<string_const str>
    struct string_type
    {
        consteval const char *c_str() const { return str.c_str(); }
        consteval size_t size() const { return str.size(); }

        consteval operator std::string_view() const { return str; }

        consteval auto begin() const { return str.begin(); }
        consteval auto end() const { return str.end(); }

        consteval bool operator==(string_type) const { return true; }
        consteval bool operator==(const char *text) const { return text == (std::string_view)str; }
    };

    namespace literals
    {
        namespace
        {
            template<string_const str>
            string_type<str> operator""_t() { return {}; }
        }
    }

    struct transparent_hash
    {
    private:
        constexpr size_t get_hash(const char *str) const
        {
            size_t hash = 0x1F1E33; // wysi
            size_t i = 0;

            while(*str != '\0')
            {
                char c = *(str++);
                hash ^= (size_t)c << (((size_t)c + i++) & 15);
            }

            return hash;
        }

    public:
        using is_transparent = void;

        size_t operator()(const std::string &str) const { return get_hash(str.data()); }
        size_t operator()(const std::string_view &str) const { return get_hash(str.data()); }

        template<string_const str>
        consteval size_t operator()(string_type<str>) const { return get_hash(str.c_str()); }
    };
}

#endif