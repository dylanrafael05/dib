#pragma once

#include "dib/preprocess.h"
#include <utility>

namespace dib::pack
{
    namespace detail
    {
        template<class... Pack, size_t... I>
        constexpr void for_each_impl(auto &&l, std::tuple<Pack...> &args, std::index_sequence<I...>)
        {
            (l(FORWARD(std::get<I>(args))), ...);
        }

        template<class... Pack>
        class Result
        {
            std::tuple<Pack...> _pack;

        public:
            explicit Result(std::tuple<Pack...> &&pack) 
                : _pack(MOVE(pack)) 
            {}

            void operator|(auto &&l) const
            {
                return dib::pack::detail::for_each_impl(FORWARD(l), _pack, std::make_index_sequence<sizeof...(Pack)>());
            }
        };
    }

    /// Helper function to iterate over all elements of a pack. 
    /// Place a lambda taking the value after this with an `|` between;
    /// i.e. for_each(pack...) | [](auto &&lmb) {}
    template<class... Pack>
    constexpr decltype(auto) for_each(Pack &&...pack)
    {
        return dib::pack::detail::Result({ FORWARD(pack)... });
    }
}