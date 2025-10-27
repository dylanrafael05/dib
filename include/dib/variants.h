#ifndef __DIB_VARS_H
#define __DIB_VARS_H

#include <variant>
#include "dib/optional.h"
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
    constexpr dib::Optional<T&> get_opt(std::variant<Vrs...> &var_like)
    {
        if (is<T>(var_like))
        {
            return dib::some<T&>(get<T>(var_like));
        }
        else
        {
            return dib::none;
        }
    }

    template<class T, class... Vrs>
    constexpr dib::Optional<const T&> get_opt(const std::variant<Vrs...> &var_like)
    {
        return get_opt<T, Vrs...>(types::remove_const(var_like));
    }

    template<class T, class... Vrs>
    constexpr dib::Optional<T &> get_opt_poly(std::variant<Vrs...> &var_like)
    {
        return std::visit([]<class V>(V & val)
        {
            if constexpr (std::is_base_of_v<T, V>)
            {
                return dib::Optional<T &>(val);
            }
            else
            {
                return dib::none_of<T &>;
            }
        }, var_like);
    }

    template<class T, class... Vrs>
    constexpr dib::Optional<const T &> get_opt_poly(const std::variant<Vrs...> &var_like)
    {
        return get_opt_poly<T, Vrs...>(dib::types::remove_const(var_like));
    }
}

#endif