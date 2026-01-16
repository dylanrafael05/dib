#pragma once

#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "dib/preprocess.h"

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
		return ((char *)(void *)&a) == ((char *)(void *)&b);
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
	struct HashProvided : Marker<HashProvided> {};

	template<class T>
	concept IsHashProvided = IsDerivedFrom<T, HashProvided> && requires(const T &t) 
	{ 
		{ t.get_hash() } -> IsSameAs<size_t>; 
	};
	
	/// A type should be marked as trivially relocatable if it can be copied
	/// from one memory location to another without creating logical errors;
	/// this is typically the case of all types, except if an associated 
	/// heap-bound object points to the object being moved (as in some 
	/// implementations of std::list<>).
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
}

namespace dib
{
	/// Produce a hash for multiple objects.
	size_t get_hash(auto &&...obj) requires (sizeof...(obj) != 0)
	{
		size_t hash_salt = 43;
		return ((std::hash<std::remove_cvref_t<decltype(obj)>>{}(obj) + (hash_salt += 71, hash_salt *= 23)) ^ ...);
	}

	/// Relocate an object from a source to an uninitialized destination;
	/// equivalent to a memmove when type is relocatable, otherwise the same as a move construction
	template<class T>
	T *uninitialized_relocate(T *source, T *dest)
	{
		if constexpr (types::is_trivially_relocatable<T>)
		{
			#ifdef __GNUC__
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wclass-memaccess"
			#endif
			std::memmove(dest, source, sizeof(T));
			#ifdef __GNUC__
			#pragma GCC diagnostic pop
			#endif
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
	T *uninitialized_relocate_n(T *source, size_t n, T *dest)
	{
		if constexpr (types::is_trivially_relocatable<T>)
		{
			#ifdef __GNUC__
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wclass-memaccess"
			#endif
			std::memmove(dest, source, sizeof(T) * n);
			#ifdef __GNUC__
			#pragma GCC diagnostic pop
			#endif
		}
		else
		{
			std::uninitialized_move_n(source, n, dest);
		}

		return dest;
	}

	/// Relocate an object from a source to an initialized destination
	template<class T>
	T *relocate(T *source, T *dest)
	{
		std::destroy_at(dest);
		uninitialized_relocate(source, dest);

		return dest;
	}

	/// Relocate an array of objects from a source to an initialized destination,
	/// where the source and destination do not overlap
	template<class T>
	T *nonoverlapping_relocate_n(T *source, size_t n, T *dest)
	{
		// Group relocation //
		std::destroy_n(dest, n);
		uninitialized_relocate_n(source, n, dest);

		return dest;
	}

	/// Relocate an array of objects from a source to an initialized destination
	template<class T>
	T *relocate_n(T *source, size_t n, T *dest)
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
	/// Get the true size of a type, which can be zero if it is void or empty
	template<class T>
	constexpr size_t packed_sizeof = []
	{
		if constexpr (std::is_void_v<T>) return 0;
		else if constexpr (std::is_empty_v<T>) return 0;
		else return sizeof(T);
	}();

	/// A description of a type's memory layout, as well as a set of methods
	/// for manipulating that type in memory (i.e. constructors and destructors).
	/// There should only ever be one instance of this type for any given type,
	/// which is provided through dib::types::TypeDescriptor::of<> or dib::types::typedesc<>
	class TypeDescriptor : public HashProvided
	{
		struct VTable
		{
			void (*_destruct)(void *) = nullptr;
			void (*_default_construct)(void *) = nullptr;
			void (*_relocate)(void *src, void *dest) = nullptr;
			void (*_move_construct)(void *src, void *dest) = nullptr;
			void (*_copy_construct)(void *src, void *dest) = nullptr;
			void (*_swap)(void *src, void *dest) = nullptr;

			const std::type_info *_type = nullptr;
			size_t _size = 0;
			size_t _packed_size = 0;
			size_t _align = 0;

			struct
			{
				bool _polymorphic : 1 = false;
				bool _trivial : 1 = false;
				bool _trivial_copy : 1 = false;
				bool _trivial_dest : 1 = false;
				bool _trivial_move : 1 = false;
				bool _trivial_rel : 1 = false;
			};

			template<class T>
			consteval static VTable generate_for_type()
			{
				VTable desc;

				if constexpr (std::is_trivially_destructible_v<T>)
				{
					desc._destruct = [](void *) {};
				}
				else
				{
					desc._destruct = [](void *value) { reinterpret_cast<T *>(value)->~T(); };
				}

				if constexpr (std::is_default_constructible_v<T>)
				{
					desc._default_construct = [](void *src)
					{
						new(src) T();
					};
				}

				if constexpr (std::is_move_constructible_v<T>)
				{
					desc._move_construct = [](void *src, void *dest)
					{
						new(dest) T(reinterpret_cast<T &&>(*reinterpret_cast<T *>(src)));
					};
				}

				if constexpr (std::is_copy_constructible_v<T>)
				{
					desc._copy_construct = [](void *src, void *dest)
					{
						new(dest) T(reinterpret_cast<T &>(*reinterpret_cast<T *>(src)));
					};
				}

				if constexpr (is_relocatable<T>)
				{
					desc._relocate = [](void *src, void *dest)
					{
						dib::uninitialized_relocate((T *)src, (T *)dest);
					};
				}

				if constexpr (std::is_swappable_v<T>)
				{
					desc._swap = [](void *src, void *dest)
					{
						std::swap(*(T *)src, *(T *)dest);
					};
				}

				desc._type = &typeid(T);
				desc._size = sizeof(T);
				desc._packed_size = packed_sizeof<T>;
				desc._align = alignof(T);

				desc._polymorphic = std::is_polymorphic_v<T>;
				desc._trivial = std::is_trivial_v<T>;
				desc._trivial_copy = std::is_trivially_copyable_v<T>;
				desc._trivial_dest = std::is_trivially_destructible_v<T>;
				desc._trivial_move = std::is_trivially_move_constructible_v<T>;
				desc._trivial_rel = dib::types::is_trivially_relocatable<T>;

				return desc;
			}
		};

		template<class T> 
		static constexpr VTable vtable_of = VTable::generate_for_type<T>();

		const VTable *_vt;

		constexpr TypeDescriptor(const VTable *_vt) : _vt(_vt) {}

	public:
		constexpr TypeDescriptor() : _vt(nullptr) {}

		bool can_relocate() const;
		bool can_default() const;
		bool can_move() const;
		bool can_copy() const;
		bool can_swap() const;

		const std::type_info &type_info() const { return *_vt->_type; }
		const char *name() const { return _vt->_type->name(); }

		size_t size() const { return _vt->_size; }
		size_t packed_size() const { return _vt->_packed_size; }
		size_t align() const { return _vt->_align; }

		bool is_polymorphic() const { return _vt->_polymorphic; }
		bool is_trivial() const { return _vt->_trivial; }
		bool is_trivially_copyable() const { return _vt->_trivial_copy; }
		bool is_trivially_movable() const { return _vt->_trivial_move; }
		bool is_trivially_relocatable() const { return _vt->_trivial_rel; }
		bool is_trivially_destructable() const { return _vt->_trivial_dest; }

		void destruct(void *dest) const;

		void uninitialized_default_construct(void *dest) const;
		void uninitialized_move_construct(void *src, void *dest) const;
		void uninitialized_copy_construct(void *src, void *dest) const;
		void uninitialized_relocate(void *src, void *dest) const;

		void default_construct(void *dest) const;
		void move_construct(void *src, void *dest) const;
		void copy_construct(void *src, void *dest) const;
		void relocate(void *src, void *dest) const;

		void swap(void *src, void *dest) const;

		template<class T>
		consteval static TypeDescriptor of() { return { &vtable_of<T> }; }

		size_t get_hash() const { return dib::get_hash(_vt); }

		bool operator==(const TypeDescriptor &other) const = default;
	};

	/// Get the type descriptor for a provided type.
	template<class T>
	constexpr TypeDescriptor typedesc = TypeDescriptor::of<T>();
}

/// Inject the implementation of std::hash for all subclasses of HashProvided
template<dib::types::IsHashProvided Hashable>
struct std::hash<Hashable>
{
	size_t operator()(const Hashable &hashable) const
	{
		return hashable.get_hash();
	}
};

/// General implementation of std::hash for enumeration types
template<dib::types::IsEnum Enum>
struct std::hash<Enum>
{
	size_t operator()(const Enum &e) const
	{
		return dib::get_hash(static_cast<std::underlying_type_t<Enum>>(e));
	}
};

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