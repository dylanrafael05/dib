#pragma once

#include "dib/preprocess.h"
#include <ranges>
#include <type_traits>

namespace dib::collections
{
    template<class T>
    concept IsBasicCollection = requires() { typename T::value_type; };

    template<IsBasicCollection T>
    using ValueOf = T::value_type;

    template<class T>
    concept HasPushBack = requires(T &t, ValueOf<T> &&x) { t.push_back(MOVE(x)); };
    template<class T>
    concept HasInsertEnd = requires(T &t, ValueOf<T> &&x) { t.insert(t.end(), MOVE(x)); };
    template<class T>
    concept HasInsert = requires(T &t, ValueOf<T> &&x) { t.insert(MOVE(x)); };

    template<class T>
    concept IsInsertable = IsBasicCollection<T> &&
        (
            HasPushBack<T> ||
            HasInsertEnd<T> ||
            HasInsert<T> ||
            false
        );

    template<IsInsertable T>
    constexpr void insert(T &collection, ValueOf<T> &&x)
    {
        if constexpr(false);
        else if constexpr(HasPushBack<T>) collection.push_back(MOVE(x));
        else if constexpr(HasInsertEnd<T>) collection.insert(collection.end(), MOVE(x));
        else if constexpr(HasInsert<T>) collection.insert(MOVE(x));
        else static_assert(false, "This should never be instantiated!");
    }
    
    template<class T>
    concept HasReserve = requires(T &t, size_t x) { t.reserve(x); };
    template<class T>
    concept HasReserveConstructor = requires(T &t, size_t x) { T(x); };
        
    template<class T>
    concept IsReservable = IsBasicCollection<T> &&
        (
            HasReserve<T> ||
            HasReserveConstructor<T> ||
            false
        );

    template<IsReservable T>
    constexpr void reserve(T &collection, size_t size)
    {
        if constexpr(false);
        else if constexpr(HasReserve<T>) collection.reserve(size);
        else if constexpr(HasReserveConstructor<T>) collection = T(size);
        else static_assert(false, "This should never be instantiated!");
    }

    template<class T>
    concept IsBasicList = true
        && std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T> // Lists must be copyable
        && requires(const T &t) { t.begin(); t.end(); }
        && (IsInsertable<T> || IsReservable<T>);
}