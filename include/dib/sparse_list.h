#ifndef __SPARSE_LIST_H
#define __SPARSE_LIST_H

#include <vector>
#include <utility>
#include <stdexcept>
#include "dib/optional.h"

namespace dib::structures
{
	template<class T, class Allocator> 
	class SparseList;

	template<class T>
	class SparseListIterator
	{
		template<class, class>
		friend class SparseList;

		using _val = std::conditional_t<std::is_const_v<T>, const dib::Optional<std::remove_const_t<T>>, dib::Optional<T>>;

		_val *_ptr;
		_val *_str;
		_val *_end;

		SparseListIterator(_val *ptr, _val *str, _val *end)
			: _ptr(ptr), _str(str), _end(end)
		{
			while (_ptr < _end && !*_ptr) _ptr++;
		}

		void increment()
		{
			do _ptr++; while (_ptr < _end && !*_ptr);
		}

		void decrement()
		{
			do _ptr--; while (_ptr >= _str && !*_ptr);
		}

	public:
		T &operator*() { return **_ptr; }
		std::add_const_t<T> &operator*() const { return **_ptr; }

		T *operator->() { return &**_ptr; }
		std::add_const_t<T> *operator->() const { return &**_ptr; }

		SparseListIterator &operator++() { increment(); return *this; }
		SparseListIterator &operator--() { decrement(); return *this; }

		SparseListIterator operator++(int) { auto c = *this; increment(); return c; }
		SparseListIterator operator--(int) { auto c = *this; decrement(); return c; }

		bool operator==(const SparseListIterator &other) const
		{
			return _ptr == other._ptr && _str == other._str && _end == other._end;
		}

		size_t index() const { return _ptr - _str; }
	};

	template<class T, class Allocator = std::allocator<T>>
	class SparseList
	{
		using ValAllocator = std::allocator_traits<Allocator>::template rebind_alloc<dib::Optional<T>>;
		using SizeAllocator = std::allocator_traits<Allocator>::template rebind_alloc<size_t>;

		std::vector<dib::Optional<T>, ValAllocator> storage;
		std::vector<size_t, SizeAllocator> free_indices;

		size_t allocate()
		{
			if (free_indices.size() > 0)
			{
				auto idx = free_indices.back();
				free_indices.pop_back();

				return idx;
			}

			storage.emplace_back();
			return storage.size() - 1;
		}

	public:
		SparseList(const Allocator &alloc = {})
			: storage(alloc), free_indices(alloc)
		{}

		using value_type = T;
		using reference = T &;
		using pointer = T *;
		using iterator = SparseListIterator<T>;
		using const_iterator = SparseListIterator<const T>;

		// RAW GETTERS //
		
		/// <summary>
		/// Get the raw storage associated with the provided index,
		/// which can either hold a value or be freed (a.k.a. having no value).
		/// </summary>
		dib::Optional<T&> get(size_t index)
		{
			if (!storage[index]) return {};
			return { storage[index] };
		}

		/// <summary>
		/// Get the raw storage associated with the provided index,
		/// which can either hold a value or be freed (a.k.a. having no value).
		/// </summary>
		dib::Optional<const T&> get(size_t index) const
		{
			if (!storage[index]) return {};
			return { storage[index] };
		}

		/// <summary>
		/// Get the value at the provided index. Throws std::bad_optional_access 
		/// if the index is freed.
		/// </summary>
		T &at(size_t index)
		{
			return *storage[index];
		}

		/// <summary>
		/// Get the value at the provided index. Throws std::bad_optional_access 
		/// if the index is freed.
		/// </summary>
		const T &at(size_t index) const
		{
			return *storage[index];
		}

		bool has_value(size_t index) const
		{
			return storage[index];
		}

		size_t insert(const T &value)
		{
			auto slot = allocate();
			
			storage[slot] = value;

			return slot;
		}

		size_t insert(T &&value)
		{
			auto slot = allocate();

			storage[slot] = std::move(value);

			return slot;
		}

		template<class... Args>
		size_t emplace(Args &&...args)
		{
			auto slot = allocate();

			storage[slot] = { std::forward<Args>(args)... };

			return slot;
		}

		void free(size_t index)
		{
			storage[index] = {};
			free_indices.push_back(index);
		}

		template<class L>
		size_t free_if(L &&lambd)
		{
			size_t count = 0;
			for (size_t i = 0; i < capacity(); i++)
			{
				if (!storage[i]) continue;

				if (std::invoke(lambd, *storage[i]))
				{
					free(i);
					count++;
				}
			}

			return count;
		}

		size_t size() const
		{
			return storage.size() - free_indices.size();
		}

		bool empty() const
		{
			return size() == 0;
		}

		/// <summary>
		/// The amount of elements this container can store
		/// without needing to update its internal sizes.
		/// </summary>
		size_t capacity() const
		{
			return storage.size();
		}

		/// <summary>
		/// The amount of elements this container can store
		/// without needing to reallocate its internal containers.
		/// </summary>
		size_t full_capacity() const
		{
			return storage.capacity();
		}

		SparseListIterator<T> begin() { return empty() ? end() : SparseListIterator<T>{ storage.data(), storage.data(), storage.data() + storage.size() }; }
		SparseListIterator<T> end() { return { storage.data() + storage.size(), storage.data(), storage.data() + storage.size() }; }

		SparseListIterator<const T> cbegin() const { return empty() ? cend() : SparseListIterator<const T>{ storage.data(), storage.data(), storage.data() + storage.size()}; }
		SparseListIterator<const T> cend() const { return { storage.data() + storage.size(), storage.data(), storage.data() + storage.size() }; }

		SparseListIterator<const T> begin() const { return cbegin(); }
		SparseListIterator<const T> end() const { return cend(); }

		SparseListIterator<T> iterator_from_index(size_t index) { return { storage.data() + index, storage.data(), storage.data() + storage.size()}; }
		SparseListIterator<const T> iterator_from_index(size_t index) const { return { storage.data() + index, storage.data(), storage.data() + storage.size() }; }

		dib::Optional<T> *data() { return storage.data(); }
		const dib::Optional<T> *data() const { return storage.data(); }
	};
}

#endif