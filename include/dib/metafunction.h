#pragma once

#include <type_traits>
#include <utility>
#include <cstddef>
#include <tuple>

#include "dib/preprocess.h"
#include "dib/types.h"

/// This header file provides utilities for working with metafunctions;
/// that is, functions which operate on types and compile-time values.
/// In this library, a metafunction is represented as an invokable expression
/// (typically constructed from lambdas which cannot be invoked in a 'real' context).
/// The syntax adopted for these functions is declared using the following macros.

/// A wrapper macro which allows for simple decalration of type functions.
/// The expected syntax is as follows:
///    <'template args to infer/match'> ('list of patterns to match arguments to metafn with') -> 'return type'
#define METACASE(...) \
    decltype([] __VA_ARGS__ { return {}; })

#define METANOWARN ::dib::types::Never auto...

/// A helper which produces a metafunction which, when passed to CallValue with a 
/// type tests if the type may be inferred via the provided template rules. Syntax is...
///    <'template args to infer/match'> ('pattern to match type argument with')
#define METATEST(...) \
    ::dib::meta::Metafunction< \
        bool (::dib::meta::Type), \
        ::dib::meta::Match< \
            decltype([] __VA_ARGS__ -> ::dib::meta::Return<::dib::meta::Auto<true>> { return {}; }), \
            decltype([](auto) -> ::dib::meta::Return<::dib::meta::Auto<false>> { return {}; }) \
        > \
    >

namespace dib::meta
{
    /// A helper class used to represent values as types.
    /// To prevent typing errors, the type must be explicitly provided.
    /// To omit the type when it is clear from the argument, use Auto<>.
    /// Exposes a public static member 'value' for retrieval of its 
    /// associated value.
    template<class T, T ValueP>
    struct Val 
    {
        constexpr static T value = ValueP;
    };

    /// A helper alias which automatically detects the type of the 
    /// value being passed.
    template<auto Value>
    using Auto = Val<decltype(Value), Value>;
    
    /// A meta-data-structure which holds an ordered list of types.
    /// Exposes a public member 'size'.
    template<class ...Types>
    struct List 
    {
        constexpr static size_t size = sizeof...(Types);
        constexpr static std::array<decltype(^^int), sizeof...(Types)> types = { ^^Types... };
    };
    
    template<class T>
    struct Return
    {};
    
    struct Type;

    template<class Type, class Decl = int>
    struct Metafunction {};
    
    /// Implementation details for the IsMetaFunction concept.
    namespace detail
    {
        template<class, class Type> struct IsMetafunctionHelper : std::false_type {};
        template<class FN, class Type> struct IsMetafunctionHelper<Metafunction<Type, FN>, Type> : std::true_type {};

        template<class T> struct MetaReturnUnwrapHelper {};
        template<class T> struct MetaReturnUnwrapHelper<Return<T>> { using Type = T; };

        template<class T> struct GetMetafunctionParameterType { using Type = Type; };
        template<class T, T Value> struct GetMetafunctionParameterType<Val<T, Value>> { using Type = T; };
        template<class FN, class Return, class ...Arguments> struct GetMetafunctionParameterType<Metafunction<Return(Arguments...), FN>> { using Type = Metafunction<Return(Arguments...)>; };
    }

    /// Check whether or not the provided type is a metafunction over the provided arguments.
    template<class FN, class Type>
    concept IsMetafunction = detail::IsMetafunctionHelper<FN, Type>::value;
    
    /// A helper which takes a metafunction as a type, and evaluates it
    /// on the provided arguments.
    template<class MetaFN, class ...Arguments> 
    using Call = MetaFN::template Call<Arguments...>;
    
    template<class MetaFN, class ...Arguments>
    constexpr static auto Eval = Call<MetaFN, Arguments...>::value;
    
    template<class Ret, class ...Args>
    struct Metafunction<Ret(Args...), void>
    {
        using Return = Ret;
        using Arguments = List<Args...>;
        
        using Self = Metafunction<Ret(Args...)>;
    };
    
    template<class Ret, class Decl, class ...Args>
    struct Metafunction<Ret(Args...), Decl> : Decl
    {
        using FN = Decl;
        using Return = Ret;
        using Arguments = List<Args...>;
        
        using Decl::operator();
        using Self = Metafunction<Ret(Args...), Decl>;

        template<class ...Arguments>
            requires IsMetafunction<Self, Ret(typename detail::GetMetafunctionParameterType<Arguments>::Type...)>
        using Call = typename detail::MetaReturnUnwrapHelper<decltype(std::declval<Self>()(std::declval<Arguments>()...))>::Type;
        
        template<class ...Arguments>
        constexpr static auto Eval = Call<Arguments...>::value;
    };
    
    /// Implementation details for the IsVal concept.
    namespace detail
    {
        template<class> struct IsValHelper : std::false_type {};
        template<class T, T V> struct IsValHelper<Val<T, V>> : std::true_type {};
    }

    /// Check whether or not the provided type holds a value.
    template<class Types>
    concept IsVal = detail::IsValHelper<Types>::value;
    

    /// Implementation details for the IsList concept
    namespace detail
    {
        template<class> struct IsListHelper : std::false_type {};
        template<class...L> struct IsListHelper<List<L...>> : std::true_type {};
    }

    /// Check if the provided type is a list.
    template<class Types>
    concept IsList = detail::IsListHelper<Types>::value;
    
    template<IsList L>
    constexpr decltype(auto) splat_list(auto &&lmb)
    {
        return [&]<class ...Types>(List<Types...>)
        {
            return lmb.template operator()<Types...>();
        }
        (L{});
    }

    template<IsList L>
    constexpr decltype(auto) each_list(auto &&lmb)
    {
        return [&]<class ...Types>(List<Types...>)
        {
            ([&]<class T>
            {
                return lmb.template operator()<T>();
            }.template operator()<Types>(), 
            ...);
        }
        (L{});
    }
    
    /// A helper type to merge many metafunctions into one,
    /// where failure to match with the signature of one metafunction
    /// is not an error but rather treated as a chance to match one of the
    /// other metafunctions provided.
    template<class ...Fns>
    struct Match : Fns...
    {
        using Fns::operator()...;
    };
    
    /// A metafunction which 'drops' the first type in a meta list,
    /// resulting in a new list with one less element. Does nothing
    /// if given an empty list.
    using ListDrop = 
        Metafunction<
            Type (Type),
            Match<
                METACASE(<class First, class... Rest>(List<First, Rest...>) -> Return<List<Rest...>>),
                METACASE((List<>, METANOWARN) -> Return<List<>>)
            >
        >;

    /// A metafunction which gets the Nth element of the provided metalist.
    /// Arguments are (type)
    using ListGet =
        Metafunction<
            Type(Type, size_t),
            Match<
                METACASE(<class First, class ...Rest>(List<First, Rest...>, Val<size_t, 0>) -> Return<First>),
                METACASE(<class Self, class First, class ...Rest, size_t N>
                    (this Self, List<First, Rest...>, Val<size_t, N>) 
                    -> Return<Call<Self, List<Rest...>, Val<size_t, N - 1>>>)
            >
        >;
        
    /// A metafunction which finds the smallest size_t N such that
    /// IsSame<ListGet<Types, N>, Type>. If no such N exists, compilation fails.
    using ListIndex = Metafunction<
        size_t (Type, Type),
        Match<
            METACASE(<class Type_, class ...Rest>(List<Type_, Rest...>, Type_) -> Return<Val<size_t, 0>>),
            METACASE(<class Self, class First, class Type_, class ...Rest>
                (this Self, List<First, Rest...>, Type_)
                -> Return<Val<size_t, Call<Self, List<Rest...>, Type_>::value + 1>>)
        >
    >;
    
    /// A metafunction which checks whether or not a metalist contains the provided type.
    using ListContains = Metafunction<
        bool (Type, Type),
        Match<
            METACASE((List<>, auto) -> Return<Auto<false>>),
            METACASE(<class Type_, class ...Rest>(List<Type_, Rest...>, Type_) -> Return<Auto<true>>),
            METACASE(<class Self, class First, class Type_, class ...Rest>
                (this Self, List<First, Rest...>, Type_)
                -> Return<Call<Self, List<Rest...>, Type_>>)
        >
    >;

    using ListAppend = Metafunction<
        Type (Type, Type),
        METACASE(<class ...L1, class ...L2>(List<L1...>, List<L2...>) -> Return<List<L1..., L2...>>)
    >;
    
    using ListMap = Metafunction<
        Type (Type, Metafunction<Type (Type)>),
        METACASE(<class... L, class E>(List<L...>, E) -> Return<List<Call<E, L>...>>)
    >;

    using ListReverse = Metafunction<
        Type (Type),
        Match<
            METACASE(<class Self, class L, class ...LR>(this Self, List<L, LR...>) 
                -> Return<ListAppend::Call<Call<Self, List<LR...>>, List<L>>>),
            METACASE((List<>, METANOWARN) -> Return<List<>>)
        >
    >;
    
    template<class InitType>
    using ListFoldr = Metafunction<
        Type (Type, Metafunction<Type (InitType, Type)>, InitType),
        Match<
            METACASE(<class Init>(List<>, auto, Init) -> Return<Init>),
            METACASE(<class Self, class T1, class Fn, class Init, class ...T>
                (this Self, List<T1, T...>, Fn, Init)
                -> Return<Call<Fn, T1, Call<Self, List<T...>, Fn, Init>>>)
        >
    >;

    /// A metafunction which checks whether or not a metalist is comprised of unique types.
    using ListIsUnique = Metafunction<
        bool (Type),
        Match<
            METACASE((List<>, METANOWARN) -> Return<Auto<true>>),
            METACASE(<class Self, class F, class ...R>(this Self, List<F, R...>) -> Return<Auto<
                !Call<ListContains, List<R...>, F>::value && Call<Self, List<R...>>::value
            >>)
        >
    >;

    /// A metafunction which gets the pointed-to type from a pointer
    using PointerGetPointee = Metafunction<
        Type (Type),
        Match<
            METACASE(<class T>(T *) -> Return<T>),
            METACASE(<class C, class T>(T C::*) -> Return<T>)
        >
    >;

    /// A metafunction which gets the return type of a function type
    using FunctionGetReturn = Metafunction<
        Type (Type),
        METACASE(<class R, class...A>(R(A...)) -> Return<R>)
    >;
    
    /// A metafunction which gets the argument types of a function type
    using FunctionGetArguments = Metafunction<
        Type (Type),
        METACASE(<class R, class...A>(R(A...)) -> Return<List<A...>>)
    >;

    /// Implementation details for splat_args
    namespace detail
    {
        template<class ArgCounter>
        constexpr decltype(auto) splat_args(
            auto &&target, 
            auto &&, 
            const std::tuple<> &, 
            auto &&...args)
        {
            return target(FORWARD(args)...);
        }
        
        template<class ArgCounter, class Arg, class ...Remaining>
        constexpr decltype(auto) splat_args(
            auto &&target, 
            auto &&arg_getter, 
            const std::tuple<Arg, Remaining...> &remaining_args, 
            auto &&...args)
        {
            auto splat_args_counted = [&]
                <size_t ...RemainingCount, size_t ...ArgCount>
                (std::index_sequence<RemainingCount...>, std::index_sequence<ArgCount...>)
            {
                auto &&arg = std::get<0>(remaining_args);
                auto rem = std::forward_as_tuple(
                    FORWARD(std::get<RemainingCount + 1>(remaining_args))...
                );

                return detail::splat_args<ArgCounter>(
                    FORWARD(target), 
                    FORWARD(arg_getter), 
                    rem, 
                    FORWARD(args)...,
                    arg_getter.template operator()<ArgCount>(FORWARD(arg))...
                );
            };

            return splat_args_counted(
                std::make_index_sequence<sizeof...(Remaining)>(),
                std::make_index_sequence<Eval<ArgCounter, Arg>>()
            );
        }
    }

    /// A helper function which takes in a target function, and two argument manipulation functions:
    /// a metafunction which counts how many target arguments a provided argument contains, and a runtime
    /// function which takes the argument index
    template<IsMetafunction<size_t(Type)> ArgCounter, class ...Arguments>
    constexpr decltype(auto) splat_args(auto &&target, auto &&arg_getter, Arguments &&...args)
    {
        return detail::splat_args<ArgCounter>(
            FORWARD(target), 
            FORWARD(arg_getter), 
            std::tuple<Arguments &&...>{FORWARD(args)...});
    }
}