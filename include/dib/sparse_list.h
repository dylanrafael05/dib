#pragma once 

#include <utility>

#include "dib/option.h"
#include "dib/preprocess.h"
#include "dib/vector.h"

/// This file defines the idea of a 'sparse list', which is analogous to a vector
/// whose elements can either be present or absent. It is stored in memory as a
/// vector of option to element type, alongside a vector of 'free' spaces.
namespace dib::structures
{
	template<class T> 
	class SparseList;

	template<class T>
	class SparseListIterator
	{
		template<class>
		friend class SparseList;

		using _val = std::conditional_t<std::is_const_v<T>, 
			const dib::option::Option<std::remove_const_t<T>>, 
			dib::option::Option<T>>;

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
		T &operator*() { return _ptr->unwrap(); }
		std::add_const_t<T> &operator*() const { return _ptr->unwrap(); }

		T *operator->() { return &operator*(); }
		std::add_const_t<T> *operator->() const { return &operator*(); }

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

	template<class T>
	class SparseList
	{
		Vector<dib::option::Option<T>> storage;
		Vector<size_t> free_indices;

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
		SparseList()
			: storage(), free_indices()
		{}

		using value_type = T;
		using reference = T &;
		using pointer = T *;
		using iterator = SparseListIterator<T>;
		using const_iterator = SparseListIterator<const T>;

		// RAW GETTERS //
		
		/// Get the raw storage associated with the provided index,
		/// which can either hold a value or be freed (a.k.a. having no value).
		dib::option::Option<T&> get(size_t index)
		{
			if (!storage[index]) return {};
			return { storage[index] };
		}

		/// Get the raw storage associated with the provided index,
		/// which can either hold a value or be freed (a.k.a. having no value).
		dib::option::Option<const T&> get(size_t index) const
		{
			if (!storage[index]) return {};
			return { storage[index] };
		}

		/// Get the value at the provided index. Throws if the index is freed.
		T &at(size_t index)
		{
			return storage[index].unwrap();
		}

		/// Get the value at the provided index. Throws if the index is freed.
		const T &at(size_t index) const
		{
			return storage[index].unwrap();
		}

		/// Check if the provided index currently has a value.
		bool has_value(size_t index) const
		{
			return storage[index];
		}

		/// Insert the provided value into this list, returning the position at which
		/// the element was inserted.
		size_t insert(const T &value)
		{
			auto slot = allocate();
			
			storage[slot] = value;

			return slot;
		}

		/// Insert the provided value into this list, returning the position at which
		/// the element was inserted.
		size_t insert(T &&value)
		{
			auto slot = allocate();

			storage[slot] = std::move(value);

			return slot;
		}

		/// Insert a new value into this list via construction, returning the position at which
		/// the element was inserted.
		template<class... Args>
		size_t emplace(Args &&...args)
		{
			auto slot = allocate();

			storage[slot] = { FORWARD(args)... };

			return slot;
		}

		/// Relinquish the value at the provided index.
		void free(size_t index)
		{
			storage[index] = {};
			free_indices.push_back(index);
		}

		/// Relinquish all values which match the provided predicate.
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

		/// The amount of elements this container can store
		/// without needing to update its internal sizes.
		size_t capacity() const
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

		dib::option::Option<T> *data() { return storage.data(); }
		const dib::option::Option<T> *data() const { return storage.data(); }
	};
}