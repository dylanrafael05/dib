#pragma once

#include <compare>
#include <concepts>
#include <type_traits>

#include "dib/metafunction.h"
#include "dib/preprocess.h"
#include "dib/record.h"
#include "dib/types.h"

namespace dib::types
{
    struct NewtypeMarker : Marker<NewtypeMarker> {};

    template<class T>
    struct [[=provides_hash]] Newtype 
        : public dib::types::TriviallyRelocatableIf<dib::types::is_trivially_relocatable<T>>
        , public NewtypeMarker
    {
    private:
        T _value;

    public:
        using ValueType = T;

        constexpr Newtype() requires (std::is_default_constructible_v<T>) 
            : _value() 
        {}
        
        constexpr Newtype(const T &value) requires (std::is_copy_constructible_v<T>) 
            : _value(value) 
        {}

        constexpr Newtype(T &&value) requires (std::is_move_constructible_v<T>) 
            : _value(MOVE(value)) 
        {}

        constexpr const T &value() const { return _value; }

        constexpr bool operator==(const Newtype<T> &other) const
            requires (types::IsEqualityComparable<T>)
        {
            return _value == other._value;
        }

        constexpr auto operator<=>(const Newtype<T> &other) const
            requires (types::IsThreeWayComparable<T>)
        {
            return _value <=> other._value;
        }

        constexpr size_t get_hash() const
            requires (types::IsHashable<T>)
        {
            return dib::get_hash(_value);
        }
    };

    template<class T>
    concept IsNewtype = std::derived_from<T, NewtypeMarker>;
}