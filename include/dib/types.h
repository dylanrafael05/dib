#ifndef __DIB_TYPE_TRAITS_H
#define __DIB_TYPE_TRAITS_H

#include <type_traits>
#include <cstddef>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "dib/preprocess.h"

namespace dib
{
	template<class T>
	constexpr std::remove_cvref_t<T> copy(T &&value)
	{
		return (std::remove_cvref_t<T>)value;
	}

	template<class T>
	constexpr std::remove_cvref_t<T> *copy_new(T &&value)
	{
		return new std::remove_cvref_t<T>(value);
	}

	template<class T>
	constexpr decltype(auto) demove(T &&value)
	{
		if constexpr (std::is_rvalue_reference_v<decltype(value)>)
		{
			return copy(DIB_FWD(value));
		}
		else return value;
	}

	template<class A, class B>
	constexpr bool ref_equal(A &&a, B &&b)
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

	template<class T, class V>
	concept cvref_eq = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<V>>;

	template<class T, class Base>
	concept not_derived_from = !std::derived_from<T, Base>;
	template<class T, class V>
	concept not_same_as = !std::same_as<T, V>;
	template<class T, class V>
	concept not_cvref_eq = !cvref_eq<T, V>;
	template<class From, class To>
	concept not_convertible_to = !std::convertible_to<From, To>;
	template<class T>
	concept not_void = !std::is_void_v<T>;

	template<class From, class To>
	struct CopyConstType { using type = std::remove_const_t<To>; };
	template<class From, class To>
	struct CopyConstType<const From, To> { using type = std::add_const_t<To>; };

	template<class From, class To>
	using copy_const_t = CopyConstType<From, To>::type;

	// Perfect marker types; implements comparison operators only for
	// the marker type itself, while deleting comparison with other object
	// types. This makes it so that comparisons can be defaulted in derived
	// classes, and that the default 'always true' comparison of the marker
	// type is not callable with derived classes.
	template<class Self>
	struct Marker {};

	template<class Mark> requires std::derived_from<Mark, Marker<Mark>>
	constexpr auto operator==(const Mark &, const Mark &) noexcept { return true; }
	template<class Mark> requires std::derived_from<Mark, Marker<Mark>>
	constexpr auto operator<=>(const Mark &, const Mark &) noexcept { return std::strong_ordering::equal; }

	template<class Mark, not_same_as<Mark> Other> requires std::derived_from<Mark, Marker<Mark>>
	void operator==(const Mark &, const Other &) = delete;
	template<class Mark, not_same_as<Mark> Other> requires std::derived_from<Mark, Marker<Mark>>
	void operator<=>(const Mark &, const Other &) = delete;

	// Custom marker types //
	struct TriviallyRelocatable : Marker<TriviallyRelocatable> {};

	template<bool> struct TriviallyRelocatableIf : TriviallyRelocatable {};
	template<> struct TriviallyRelocatableIf<false> {};

	struct HashProvided : Marker<HashProvided> {};

	template<class T>
	struct IsTriviallyRelocatableType
	{
		constexpr static bool value = std::is_trivially_move_constructible_v<T> || std::is_base_of_v<TriviallyRelocatable, T>;
	};

	template<class T>
	constexpr bool is_trivially_relocatable = IsTriviallyRelocatableType<T>::value;

	template<class T>
	constexpr bool is_relocatable = is_trivially_relocatable<T> || std::is_move_constructible_v<T>;
}

namespace dib
{
	size_t get_hash(auto &&...obj) requires (sizeof...(obj) != 0)
	{
		size_t hash_salt = 43;
		return ((std::hash<std::remove_cvref_t<decltype(obj)>>{}(obj) + (hash_salt += 71, hash_salt *= 23)) ^ ...);
	}

	template<class T>
	T *uninitialized_relocate(T *source, T *dest)
	{
		if constexpr (types::is_trivially_relocatable<T>)
		{
			std::memmove(dest, source, sizeof(T));
		}
		else
		{
			std::construct_at(dest, std::move(*source));
		}

		return dest;
	}

	template<class T>
	T *uninitialized_relocate_n(T *source, size_t n, T *dest)
	{
		if constexpr (types::is_trivially_relocatable<T>)
		{
			std::memmove(dest, source, sizeof(T) * n);
		}
		else
		{
			std::uninitialized_move_n(source, n, dest);
		}

		return dest;
	}

	template<class T>
	T *relocate(T *source, T *dest)
	{
		std::destroy_at(dest);
		uninitialized_relocate(source, dest);

		return dest;
	}

	template<class T>
	T *nonoverlapping_relocate_n(T *source, size_t n, T *dest)
	{
		// Group relocation //
		std::destroy_n(dest, n);
		uninitialized_relocate_n(source, n, dest);

		return dest;
	}

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
	template<class T>
	constexpr size_t packed_sizeof = []
		{
			if constexpr (std::is_void_v<T>) return 0;
			else if constexpr (std::is_empty_v<T>) return 0;
			else return sizeof(T);
		}
	();

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

	template<class T>
	constexpr TypeDescriptor typedesc = TypeDescriptor::of<T>();
}

template<std::derived_from<dib::types::HashProvided> Hashable>
	requires requires(const Hashable &hashable) { { hashable.get_hash() } -> std::same_as<size_t>; }
struct std::hash<Hashable>
{
	size_t operator()(const Hashable &hashable) const
	{
		return hashable.get_hash();
	}
};

template<class Enum>
	requires std::is_enum_v<Enum>
struct std::hash<Enum>
{
	size_t operator()(const Enum &e) const
	{
		return dib::get_hash(static_cast<std::underlying_type_t<Enum>>(e));
	}
};

#define DIB_NONGENERIC_INJECT template<>
#define DIB_GENERIC_INJECT template
#define DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(...) struct ::dib::types::IsTriviallyRelocatableType<__VA_ARGS__> : ::std::true_type {}

DIB_NONGENERIC_INJECT DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::string);
DIB_GENERIC_INJECT<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::vector<T>);
DIB_GENERIC_INJECT<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unordered_set<T>);
DIB_GENERIC_INJECT<class K, class V> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unordered_map<K, V>);
DIB_GENERIC_INJECT<class T, size_t N> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::array<T, N>);
DIB_GENERIC_INJECT<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::unique_ptr<T>);
DIB_GENERIC_INJECT<class T> DIB_INJECT_TYPE_TRIVIALLY_RELOCATABLE(std::shared_ptr<T>);

#endif