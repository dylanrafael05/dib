#pragma once

#include "dib/metautils.h"
#include "dib/json.attr.h"
#include <meta>

namespace dib
{
    struct HashAsRecord {};
    struct CompareAsRecord {};
    struct Record : HashAsRecord, CompareAsRecord, json::DeriveJson {};

    constexpr HashAsRecord hash_as_record;
    constexpr CompareAsRecord compare_as_record;
    constexpr Record record;
    
    struct HashByKey {};
    struct CompareByKey {};
    struct RecordByKey : HashByKey, CompareByKey {};
    struct Key {};

    constexpr HashByKey hash_by_key;
    constexpr CompareByKey compare_by_key;
    constexpr RecordByKey record_by_key;
    constexpr Key key;

    struct ProvidesHash {};

    constexpr ProvidesHash provides_hash;
}

/// Generate a hash memberwise for any type annotated as opting into
/// this behaviour.
template<dib::AnnotatedWith<dib::HashAsRecord> T>
struct std::hash<T> 
{
    constexpr size_t operator()(const T &value) const
    {
        size_t hash = 0;

        template for(constexpr auto field : std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
        {
            constexpr auto type = std::meta::type_of(field);

            hash ^= std::hash<typename [:type:]>{}(value.[:field:]);
            hash *= 973;
        }

        return hash;
    }
};

/// Generate a hash that delegates to a single member if requested.
template<dib::AnnotatedWith<dib::HashByKey> T>
struct std::hash<T> 
{
    constexpr size_t operator()(const T &value) const
    {
        size_t hash = 0;

        template for(constexpr auto field : std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
        {
            if constexpr(dib::has_annotation(field, ^^dib::Key))
            {
                constexpr auto type = std::meta::type_of(field);
                hash = std::hash<typename [:type:]>{}(value.[:field:]);;
            }
        }

        return hash;
    }
};

/// Inject the implementation of std::hash for all subclasses of HashProvided.
/// Note that we need to double-check that our hash is actually provided, since
/// template arguments may cause get_hash to be only conditionally supported,
/// and we cannot introspect onto template arguments at definition time.
template<dib::AnnotatedWith<dib::ProvidesHash> Hashable> requires requires(const Hashable &h) { h.get_hash(); }
struct std::hash<Hashable>
{
	constexpr size_t operator()(const Hashable &hashable) const
	{
		return hashable.get_hash();
	}
};

/// Generate an equality check memberwise for any type that opts into it.
/// This is done out-of-line to reduce syntax burden of default-declaring
/// operator==.
template<dib::AnnotatedWith<dib::CompareAsRecord> T>
constexpr bool operator==(const T &lhs, const T &rhs)
{
    template for(constexpr auto field : std::define_static_array(
        std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
    {
        if(!(lhs.[:field:] == rhs.[:field:]))
            return false;
    }

    return true;
}

/// Generate an equality check that delegates to a single member if requested.
template<dib::AnnotatedWith<dib::CompareByKey> T>
constexpr bool operator==(const T &lhs, const T &rhs)
{
    template for(constexpr auto field : std::define_static_array(
        std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
    {
        if constexpr (dib::has_annotation(field, ^^dib::Key))
        {
            return lhs.[:field:] == rhs.[:field:];
        }
    }
}
