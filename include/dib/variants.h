#pragma once

#include <variant>

#include "dib/option.h"
#include "dib/types.h"

namespace dib::vars
{
    template<class T, class... Vrs>
    consteval size_t index_of_type_fn()
    {
        size_t final_val = (size_t)(-1);
        size_t x = 0;

        ([&] {
            if constexpr (std::is_same_v<T, Vrs>)
            {
                final_val = x;
            }
            x++;
        }(), ...);


        return final_val;
    }

    template<class T, class... Vrs>
    constexpr size_t index_of_type = index_of_type_fn<T, Vrs...>();

    template<class T, class... Vrs>
    constexpr bool is(const std::variant<Vrs...> &var_like)
    {
        return var_like.index() == index_of_type<T, Vrs...>;
    }

    template<class T, class... Vrs>
    constexpr bool is_poly(const std::variant<Vrs...> &var_like)
    {
        return std::visit([]<class V>(const V & _)
        {
            return std::is_base_of_v<T, V>;
        }, var_like);
    }

    template<class T, class... Vrs>
    constexpr dib::option::Option<T&> get_opt(std::variant<Vrs...> &var_like)
    {
        if (is<T>(var_like))
        {
            return dib::option::some<T&>(get<T>(var_like));
        }
        else
        {
            return dib::option::none;
        }
    }

    template<class T, class... Vrs>
    constexpr dib::option::Option<const T&> get_opt(const std::variant<Vrs...> &var_like)
    {
        return get_opt<T, Vrs...>(types::remove_const(var_like));
    }

    template<class T, class... Vrs>
    constexpr dib::option::Option<T &> get_opt_poly(std::variant<Vrs...> &var_like)
    {
        return std::visit([]<class V>(V & val)
        {
            if constexpr (std::is_base_of_v<T, V>)
            {
                return dib::option::Option<T &>(val);
            }
            else
            {
                return dib::option::none_of<T &>;
            }
        }, var_like);
    }

    template<class T, class... Vrs>
    constexpr dib::option::Option<const T &> get_opt_poly(const std::variant<Vrs...> &var_like)
    {
        return get_opt_poly<T, Vrs...>(dib::types::remove_const(var_like));
    }
}