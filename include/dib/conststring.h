#ifndef __DIB_CONSTSTRINGS_H
#define __DIB_CONSTSTRINGS_H

#include <stddef.h>
#include <string>
#include <string_view>
#include <meta>

namespace dib::strings
{
    class StringLiteral
    {
        const char *ptr;
        size_t length;

    public:
        consteval StringLiteral(const char *str)
        {
            ptr = str;
            length = 0;
            
            for(const char *it = ptr; *it != '\0'; it++)
                length++;
        }
        
        consteval explicit StringLiteral(std::string_view str)
        {
            ptr = str.data();
            length = str.length();
        }

        constexpr StringLiteral()
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

    bool operator==(StringLiteral, const char *);
    bool operator==(const char *, StringLiteral);
    
    template<size_t N>
    struct StringConst
    {
        char text[N];

        constexpr StringConst(const char (&text)[N])
        {
            for(size_t i = 0; i < N; i++)
                this->text[i] = text[i];
        }
        constexpr explicit StringConst(const char *text)
        {
            for(size_t i = 0; i < N - 1; i++)
                this->text[i] = text[i];

            this->text[N - 1] = 0;
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

    constexpr auto strlen(const char *s) -> size_t
    {
        size_t i;
        for(i = 0uz; s[i] != 0; i++);
        return i;
    }
    
    template<const char *str>
    consteval auto make_const()
    {
        constexpr auto strtype = std::meta::substitute(^^StringConst, { std::meta::reflect_constant(strlen(str) + 1) });
        return typename [: strtype :](str);
    }

    template<StringConst str>
    struct StringType
    {
        consteval const char *c_str() const { return str.c_str(); }
        consteval size_t size() const { return str.size(); }

        consteval operator std::string_view() const { return str; }

        consteval auto begin() const { return str.begin(); }
        consteval auto end() const { return str.end(); }

        consteval bool operator==(StringType) const { return true; }
        consteval bool operator==(const char *text) const { return text == (std::string_view)str; }
    };

    namespace literals
    {
        namespace
        {
            template<StringConst str>
            StringType<str> operator""_t() { return {}; }
        }
    }

    struct TransparentHash
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

        template<StringConst str>
        consteval size_t operator()(StringType<str>) const { return get_hash(str.c_str()); }
    };
}

#endif