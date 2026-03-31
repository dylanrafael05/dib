#pragma once

#include <compare>
#include <concepts>
#include <list>
#include <map>
#include <set>
#include <string_view>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <string>
#include <memory>

#include "dib/preprocess.h"
#include "dib/nameof.h"

namespace dib
{
	/// Create a copy of the provided value, via the copy constructor.
	template<class T>
	constexpr std::remove_cvref_t<T> copy(T &&value)
	{
		return (std::remove_cvref_t<T>)value;
	}

	/// Create a copy of the provided value allocated on the heap, via the copy constructor.
	template<class T>
	constexpr std::remove_cvref_t<T> *copy_new(T &&value)
	{
		return new std::remove_cvref_t<T>(value);
	}

	/// Transform to a non-r-value reference
	constexpr decltype(auto) demove(auto &&value)
	{
		if constexpr (std::is_rvalue_reference_v<decltype(value)>)
		{
			return copy(FORWARD(value));
		}
		else return value;
	}

	/// Check if two references refer to the same memory location
	constexpr bool ref_equal(auto &&a, auto &&b)
	{
		return (uintptr_t)std::addressof(a) == (uintptr_t)std::addressof(b);
	}
}

namespace dib::types
{
	template<class T>
	constexpr T &remove_const(const T &val) { return const_cast<T &>(val); }
	template<class T>
	constexpr T &remove_const(T &val) { return val; }

	template<class T>
	constexpr const T &add_const(T &val) { return val; }
	template<class T>
	constexpr const T &add_const(const T &val) { return val; }

	/// Concept aliases

	template<class T, class Base>
	concept IsDerivedFrom = std::derived_from<T, Base>;
	template<class T, class V>
	concept IsSameAs = std::same_as<T, V>;
    template<class LHS, class RHS>
    concept IsValueSame = IsSameAs<std::remove_cvref_t<LHS>, std::remove_cvref_t<RHS>>;
	template<class T, class V>
	concept IsCVRefEq = IsSameAs<std::remove_cvref_t<T>, std::remove_cvref_t<V>>;
	template<class T, class V>
	concept IsConvertibleTo = std::convertible_to<T, V>;
	template<class T>
	concept IsVoid = std::is_void_v<T>;
	template<class T>
	concept IsZST = std::is_void_v<T> || std::is_empty_v<T>;
	template<class T>
	concept IsSized = !IsZST<T>;
	template<class T>
	concept IsEnum = std::is_enum_v<T>;
	template<class T>
	concept IsValue = !std::is_reference_v<T>;
	template<class T>
	concept IsPointer = std::is_pointer_v<T>;

	template<class T, class Base>
	concept NotDerivedFrom = !IsDerivedFrom<T, Base>;
	template<class T, class V>
	concept NotSameAs = !IsSameAs<T, V>;
    template<class LHS, class RHS>
    concept NotValueSame = !std::same_as<std::remove_cvref_t<LHS>, std::remove_cvref_t<RHS>>;
	template<class T, class V>
	concept NotCVRefEq = !IsCVRefEq<T, V>;
	template<class From, class To>
	concept NotConvertibleTo = !IsConvertibleTo<From, To>;
	template<class T>
	concept NotVoid = !IsVoid<T>;
	template<class T>
	concept NotEnum = !IsEnum<T>;
	template<class T>
	concept NotValue = !IsValue<T>;
	template<class T>
	concept NotPointer = !IsPointer<T>;
	
	template<class T>
	concept Always = true;
	template<class T>
	concept Never = false;

	/// Custom metaclasses

	template<class From, class To>
	struct CopyConstType { using type = std::remove_const_t<To>; };
	template<class From, class To>
	struct CopyConstType<const From, To> { using type = std::add_const_t<To>; };

	template<class From, class To>
	using CopyConst = CopyConstType<From, To>::type;
	
	template<class From, class To>
	struct CopyRefType { using type = std::remove_reference_t<To>; };
	template<class From, class To>
	struct CopyRefType<From &, To> { using type = std::add_lvalue_reference_t<To>; };
	template<class From, class To>
	struct CopyRefType<From &&, To> { using type = std::add_rvalue_reference_t<To>; };
	
	template<class From, class To>
	using CopyRef = CopyRefType<From, To>::type;
	template<class From, class To>
	using CopyConstRef = CopyRef<From, CopyConst<std::remove_reference_t<From>, To>>;

	// Perfect marker types; implements comparison operators only for
	// the marker type itself, while deleting comparison with other object
	// types. This makes it so that comparisons can be defaulted in derived
	// classes, and that the default 'always true' comparison of the marker
	// type is not callable with derived classes. Note this if this was omitted,
	// classes which derive from the marker classes would not be able to default
	// comparisons or would do so in an incorrect manner.
	template<class Self>
	struct Marker {};

	template<class Mark> requires std::derived_from<Mark, Marker<Mark>>
	constexpr auto operator==(const Mark &, const Mark &) noexcept { return true; }
	template<class Mark> requires std::derived_from<Mark, Marker<Mark>>
	constexpr auto operator<=>(const Mark &, const Mark &) noexcept { return std::strong_ordering::equal; }

	template<class Mark, NotSameAs<Mark> Other> requires std::derived_from<Mark, Marker<Mark>>
	void operator==(const Mark &, const Other &) = delete;
	template<class Mark, NotSameAs<Mark> Other> requires std::derived_from<Mark, Marker<Mark>>
	void operator<=>(const Mark &, const Other &) = delete;

	// Custom marker types //

	/// A type that provides the std::hash functionality via a member .get_hash()
	/// should inherit this type.
	struct [[deprecated("Reflection supercedes use of this type")]] HashProvided : Marker<HashProvided> {};

	template<class T>
	concept IsHashProvided [[deprecated("Reflection supercedes use of this type")]] = IsDerivedFrom<T, HashProvided> && requires(const T &t) 
	{ 
		{ t.get_hash() } -> IsSameAs<size_t>; 
	};
	
	/// A type should be marked as trivially relocatable if it can be copied
	/// from one memory location to another without creating logical errors;
	/// this is typically the case of all types, except if an associated 
	/// heap-bound object points to the object being moved (as in some 
	/// implementations of std::list<>).

	// TODO; reimplement via annotation
	struct TriviallyRelocatable : Marker<TriviallyRelocatable> {};

	template<bool> struct TriviallyRelocatableIf : TriviallyRelocatable {};
	template<> struct TriviallyRelocatableIf<false> {};

	/// A metaclass for trivially relocatable types; see dib::types::TrviallyRelocatable
	/// for more information about relocatable types.
	template<class T>
	struct IsTriviallyRelocatableType
	{
		constexpr static bool value = std::is_trivially_move_constructible_v<T> || std::is_base_of_v<TriviallyRelocatable, T>;
	};

	template<class T>
	constexpr bool is_trivially_relocatable = IsTriviallyRelocatableType<T>::value;
	template<class T>
	concept IsTriviallyRelocatable = is_trivially_relocatable<T>;

	template<class T>
	constexpr bool is_relocatable = is_trivially_relocatable<T> || std::is_move_constructible_v<T>;
	template<class T>
	concept IsRelocatable = is_relocatable<T>;

	/// Annoyingly, standard containers dont hide operators that dont compile, nor can we check
	/// automatically if the functions will compile. So we need to include all of their headers
	/// and nuke our compile time to detect them.

	template<class T>
	struct IsEqualityComparableType : std::bool_constant<std::equality_comparable<T>> {};
	template<class ...T>
	struct IsEqualityComparableType<std::tuple<T...>> : std::bool_constant<(IsEqualityComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsEqualityComparableType<std::pair<T...>> : std::bool_constant<(IsEqualityComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsEqualityComparableType<std::vector<T...>> : std::bool_constant<(IsEqualityComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsEqualityComparableType<std::list<T...>> : std::bool_constant<(IsEqualityComparableType<T>::value && ...)> {};
	template<class T, class E, class A>
	struct IsEqualityComparableType<std::set<T, E, A>> : std::bool_constant<(IsEqualityComparableType<T>::value && IsEqualityComparableType<A>::value)> {};
	template<class K, class V, class E, class A>
	struct IsEqualityComparableType<std::map<K, V, E, A>> : std::bool_constant<(IsEqualityComparableType<K>::value && IsEqualityComparableType<V>::value && IsEqualityComparableType<A>::value)> {};
	template<class K, class H, class E, class A>
	struct IsEqualityComparableType<std::unordered_set<K, H, E, A>> : std::bool_constant<(IsEqualityComparableType<K>::value && IsEqualityComparableType<A>::value)> {};
	template<class K, class V, class H, class E, class A>
	struct IsEqualityComparableType<std::unordered_map<K, V, H, E, A>> : std::bool_constant<(IsEqualityComparableType<K>::value && IsEqualityComparableType<V>::value && IsEqualityComparableType<A>::value)> {};
	template<class T>
	concept IsEqualityComparable = IsEqualityComparableType<T>::value;
	
	template<class T>
	struct IsThreeWayComparableType : std::bool_constant<std::three_way_comparable<T>> {};
	template<class ...T>
	struct IsThreeWayComparableType<std::tuple<T...>> : std::bool_constant<(IsThreeWayComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsThreeWayComparableType<std::pair<T...>> : std::bool_constant<(IsThreeWayComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsThreeWayComparableType<std::vector<T...>> : std::bool_constant<(IsThreeWayComparableType<T>::value && ...)> {};
	template<class ...T>
	struct IsThreeWayComparableType<std::list<T...>> : std::bool_constant<(IsThreeWayComparableType<T>::value && ...)> {};
	template<class T, class E, class A>
	struct IsThreeWayComparableType<std::set<T, E, A>> : std::bool_constant<(IsThreeWayComparableType<T>::value && IsThreeWayComparableType<A>::value)> {};
	template<class K, class V, class E, class A>
	struct IsThreeWayComparableType<std::map<K, V, E, A>> : std::bool_constant<(IsThreeWayComparableType<K>::value && IsThreeWayComparableType<V>::value && IsThreeWayComparableType<A>::value)> {};
	template<class K, class H, class E, class A>
	struct IsThreeWayComparableType<std::unordered_set<K, H, E, A>> : std::bool_constant<(IsThreeWayComparableType<K>::value && IsThreeWayComparableType<A>::value)> {};
	template<class K, class V, class H, class E, class A>
	struct IsThreeWayComparableType<std::unordered_map<K, V, H, E, A>> : std::bool_constant<(IsThreeWayComparableType<K>::value && IsThreeWayComparableType<V>::value && IsThreeWayComparableType<A>::value)> {};
	template<class T>
	concept IsThreeWayComparable = IsThreeWayComparableType<T>::value;

	template<class T>
	concept IsHashable = requires(const T &value)
	{
		{ std::hash<std::remove_cvref_t<T>>{}(value) } -> IsSameAs<size_t>;
	};
}

namespace dib
{
	/// Produce a hash for multiple objects.
	constexpr size_t get_hash(auto &&...obj) requires (sizeof...(obj) != 0)
	{
		size_t hash_salt = 43;
		size_t result = 0;

		([&]{
			result ^= std::hash<std::remove_cvref_t<decltype(obj)>>{}(obj) ^ hash_salt;
			hash_salt += 71;
			hash_salt *= 23;
		}(), ...);

		return result;
	}

	/// Relocate an object from a source to an uninitialized destination;
	/// equivalent to a memmove when type is relocatable, otherwise the same as a move construction
	template<class T>
	constexpr T *uninitialized_relocate(T *source, T *dest)
	{
		if !consteval
		{
			if constexpr (types::is_trivially_relocatable<T>)
			{
				#ifdef DIBCOMPILER_gcc
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wclass-memaccess"
				#endif

				#ifdef DIBCOMPILER_clang
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wnontrivial-memcall"
				#endif

				std::memmove(dest, source, sizeof(T));

				#if defined(DIBCOMPILER_gcc) || defined(DIBCOMPILER_clang)
				#pragma GCC diagnostic pop
				#endif
			}
			else
			{
				std::construct_at(dest, std::move(*source));
			}
		}
		else
		{
			std::construct_at(dest, std::move(*source));
		}

		return dest;
	}

	/// Relocate an array of objects from a source to an uninitialized destination;
	/// equivalent to a memmove when type is relocatable, otherwise the same as std::uninitialized_move_n
	template<class T>
	constexpr T *uninitialized_relocate_n(T *source, size_t n, T *dest)
	{
		if !consteval
		{
			if constexpr (types::is_trivially_relocatable<T>)
			{
				#ifdef DIBCOMPILER_gcc
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wclass-memaccess"
				#endif
				
				#ifdef DIBCOMPILER_clang
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wnontrivial-memcall"
				#endif
				
				std::memmove(dest, source, sizeof(T) * n);
				
				#if defined(DIBCOMPILER_gcc) || defined(DIBCOMPILER_clang)
				#pragma GCC diagnostic pop
				#endif
			}
			else
			{
				std::uninitialized_move_n(source, n, dest);
			}
		}
		else
		{
			std::uninitialized_move_n(source, n, dest);
		}

		return dest;
	}

	/// Relocate an object from a source to an initialized destination
	template<class T>
	constexpr T *relocate(T *source, T *dest)
	{
		std::destroy_at(dest);
		uninitialized_relocate(source, dest);

		return dest;
	}

	/// Relocate an array of objects from a source to an initialized destination,
	/// where the source and destination do not overlap
	template<class T>
	constexpr T *nonoverlapping_relocate_n(T *source, size_t n, T *dest)
	{
		// Group relocation //
		std::destroy_n(dest, n);
		uninitialized_relocate_n(source, n, dest);

		return dest;
	}

	/// Relocate an array of objects from a source to an initialized destination
	template<class T>
	constexpr T *relocate_n(T *source, size_t n, T *dest)
	{
		auto diff = source - dest;

		if (-diff > (intmax_t)n || diff > (intmax_t)n) [[likely]]
		{
			nonoverlapping_relocate_n(source, n, dest);
		}
		else if (diff > 0)
		{
			// Overlap is at the start of array; split into two relocations //
			// 
			//  src:   [----]
			//  dst: [xxxx]
			//  
			//  reloc ^^
			//  reloc   ^^^^
			//
			nonoverlapping_relocate_n(source, diff, dest);
			nonoverlapping_relocate_n(source + diff, n - diff, dest + diff);
		}
		else if (diff < 0)
		{
			// Overlap is at the end of array; split into two relocations, with the later one first //
			// 
			//  src: [----]
			//  dst:   [xxxx]
			//  
			//  reloc   ^^^^
			//  reloc ^^
			//
			diff *= -1;
			nonoverlapping_relocate_n(source + diff, n - diff, dest + diff);
			nonoverlapping_relocate_n(source, diff, dest);
		}
		else
		{
			// Full overlap; no-op //
		}

		return dest;
	}
}

namespace dib::types
{
	namespace detail
	{
		static inline void trivial_fn(void *) {}
	}
	
	template<class T>
	constexpr size_t sizeof_ = []
	{
		if constexpr (std::is_function_v<T>) return 0;
		else if constexpr (std::is_void_v<T>) return 1;
		else return sizeof(T);
	}();
	
	template<class T>
	constexpr size_t alignof_ = []
	{
		if constexpr (std::is_function_v<T>) return 1;
		else if constexpr (std::is_void_v<T>) return 1;
		else return alignof(T);
	}();

	/// Get the true size of a type, which can be zero if it is void or empty
	template<class T>
	constexpr size_t packed_sizeof = []
	{
		if constexpr (std::is_void_v<T>) return 0;
		else if constexpr (std::is_empty_v<T>) return 0;
		else if constexpr (std::is_function_v<T>) return 0;
		else return sizeof(T);
	}();
}

/// General implementation of std::hash for enumeration types
// template<dib::types::IsEnum Enum>
// struct std::hash<Enum>
// {
// 	size_t operator()(const Enum &e) const
// 	{
// 		return dib::get_hash(static_cast<std::underlying_type_t<Enum>>(e));
// 	}
// };

/// Trivially relocatable standard library types
#define DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(...) \
	struct dib::types::IsTriviallyRelocatableType<__VA_ARGS__> : ::std::true_type {}

	template<>        DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::string);
	template<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::vector<T>);
	template<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unordered_set<T>);
	template<class K, class V> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unordered_map<K, V>);
	template<dib::types::IsTriviallyRelocatable T, size_t N> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::array<T, N>);
	template<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unique_ptr<T>);
	template<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::shared_ptr<T>);

#undef DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE