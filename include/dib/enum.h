#pragma once

#include <meta>
#include <algorithm>
#include <ranges>
#include <type_traits>

namespace dib::enums
{
    template<class T> requires std::is_enum_v<T>
    constexpr std::underlying_type_t<T> min_value = 
        std::meta::enumerators_of(^^T) 
        | std::ranges::transform([](auto &&info) { return std::meta::extract<std::underlying_type_t<T>>(info); })
        | std::ranges::min_element;

    template<class T> requires std::is_enum_v<T>
    constexpr std::underlying_type_t<T> max_value = 
        std::meta::enumerators_of(^^T) 
        | std::ranges::transform([](auto &&info) { return std::meta::extract<std::underlying_type_t<T>>(info); })
        | std::ranges::max_element;

    template<class T> requires std::is_enum_v<T>
    constexpr T next(T value)
    {
        constexpr auto enums = std::define_static_array(std::meta::enumerators_of(^^T));
        template for(constexpr auto e : std::ranges::views::iota(0uz, enums.size()))
        {
            if(value == [:enums[e]:])
            {
                return [:enums[(e+1) % enums.size()]:];
            }
        }

        return value;
    }
    
    template<class T> requires std::is_enum_v<T>
    constexpr void move_next(T &value)
    {
        value = next(value);
    }
}