#pragma once 

#include <cstddef>
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <memory>

#include "dib/types.h"
#include "dib/debug.h"
#include "dib/algorithm.h"

namespace dib::structures
{
	template<class T, size_t Cap>
	class Vector;

	template<class T>
	class DArray
	{
		T *_data;
		size_t _size;

		void move(DArray &&other)
		{
			_data = other._data;
			_size = other._size;

			other._data = nullptr;
			other._size = 0;
		}

		void copy(const DArray &other)
		{
			_data = new T[other._size];
			_size = other._size;
			
			std::uninitialized_copy_n(other.begin(), other.size(), begin());
		}

		void destroy()
		{
			delete[] _data;
		}

	public:
		using size_type = size_t;
		using iterator = algorithm::BasicRandomAccessIterator<DArray<T>>;
		using const_iterator = algorithm::BasicRandomAccessIterator<const DArray<T>>;

		DArray() : _data(nullptr), _size(0) {}
		DArray(size_t count) : _data(new T[count]), _size(count) {}
		DArray(const DArray &other) { copy(other); }
		DArray(DArray &&other) { move(MOVE(other)); }

		DArray &operator=(const DArray &other)
		{
			if(ref_equal(*this, other))
				return *this;

			destroy();
			copy(other);
			return *this;
		}
		
		DArray &operator=(DArray &&other)
		{
			if(ref_equal(*this, other))
				return *this;

			destroy();
			move(MOVE(other));
			return *this;
		}

		T *data() { return _data; }
		const T *data() const { return _data; }

		size_type size() const { return _size; }
		bool empty() const { return _size == 0; }
		
		iterator begin() { return iterator(this, 0); }
		const_iterator begin() const { return const_iterator(this, 0); }
		const_iterator cbegin() const { return const_iterator(this, 0); }

		iterator end() { return begin() + size(); }
		const_iterator end() const { return begin() + size(); }
		const_iterator cend() const { return begin() + size(); }

		T &operator[](size_t n) { return *(begin() + n); }
		const T &operator[](size_t n) const { return *(begin() + n); }
		
	};

	/// A vector which can store a specified capacity of elements,
	/// and does so in-memory without allocation.
	template<class T, size_t Cap>
	class StaticVector : public types::TriviallyRelocatable
	{
		alignas(T) char _data[sizeof(T) * Cap];
		size_t _size;

		void assert_growable() const
		{
			if (_size >= Cap)
				RUNTIME_ERROR("Cannot add to full StaticVector");
		}

		void destroy()
		{
			std::destroy_n(_begin(), _size);
		}

		void copy(const StaticVector &other)
		{
			_size = other.size();
			std::uninitialized_copy_n(other._begin(), other.size(), _begin());
		}

		void move(StaticVector &&other)
		{
			_size = other.size();
			dib::uninitialized_relocate_n(other._begin(), other.size(), _begin());
		}

		T *_begin() { return (T *) &_data; }
		const T *_begin() const { return (T *) &_data; }

		T *_end() { return _begin() + size(); }
		const T *_end() const { return _begin() + size(); }

		template<class, size_t>
		friend class dib::structures::Vector;

	public:
		using size_type = size_t;
		using iterator = algorithm::BasicRandomAccessIterator<StaticVector<T, Cap>>;
		using const_iterator = algorithm::BasicRandomAccessIterator<const StaticVector<T, Cap>>;

		StaticVector() : _size(0) {}
		StaticVector(const StaticVector &other) { copy(other); }
		StaticVector(StaticVector &&other) { move(MOVE(other)); }

		StaticVector &operator=(const StaticVector &other)
		{
			if (ref_equal(*this, other)) return *this;

			destroy();
			copy(other);
			return *this;
		}

		StaticVector &operator=(StaticVector &&other)
		{
			if (ref_equal(*this, other)) return *this;

			destroy();
			move(MOVE(other));
			return *this;
		}

		~StaticVector() { destroy(); }

		T *data() { return reinterpret_cast<T*>(_data); }
		const T *data() const { return reinterpret_cast<const T*>(_data); }

		size_type size() const { return _size; }
		bool empty() const { return _size == 0; }
		bool full() const { return size() == Cap; }

		iterator begin() { return iterator(this, 0); }
		const_iterator begin() const { return const_iterator(this, 0); }
		const_iterator cbegin() const { return const_iterator(this, 0); }

		iterator end() { return begin() + size(); }
		const_iterator end() const { return begin() + size(); }
		const_iterator cend() const { return begin() + size(); }

		T &operator[](size_t n) { return *(begin() + n); }
		const T &operator[](size_t n) const { return *(begin() + n); }

		T &front() { return *begin(); }
		const T &front() const { return *begin(); }

		T &back() { return *(end() - 1); }
		const T &back() const { return *(end() - 1); }

		void push_back(const T &value) { emplace(end(), value); }
		void push_back(T &&value) { emplace(end(), MOVE(value)); }

		void insert(iterator it, const T &value) { emplace(it, value); }
		void insert(iterator it, T &&value) { emplace(it, MOVE(value)); }

		template<class... Args>
		T &emplace_back(Args &&...args) { return emplace(end(), FORWARD(args)...); }

		template<class... Args>
		T &emplace(iterator it, Args &&...args)
		{
			// Grow vector internally //
			assert_growable();

			// Perform emplacement //
			algorithm::array_emplace(_begin() + it.index(), _end(), FORWARD(args)...);
			_size++;

			// Return reference //
			return *it;
		}

		void pop_back()
		{
			erase(end() - 1);
		}

		void erase(const_iterator cit)
		{
			// Shift elements to cover removed //
			algorithm::array_erase(_begin() + cit.index(), _end());
			_size--;
		}

		/// <summary>
		/// Relocate elements from the provided contiguous array into this vector.
		/// </summary>
		void relocate_from(T *pointer, size_t n)
		{
			// Throw if relocation is not possible //
			if (n > Cap) 
				RUNTIME_ERROR("Cannot fit {} elements in a static vector of size {}", n, Cap);

			// Relocate elements (note that relocate_n isn't applicable here, since _size != n) //
			std::destroy_n(_begin(), _size);
			dib::uninitialized_relocate_n(_begin(), n, pointer);
			
			_size = n;
		}

		auto operator<=>(const StaticVector &other) const
		{
			return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
		}

		bool operator==(const StaticVector &other) const
		{
			return std::equal(begin(), end(), other.begin());
		}
	};

	/// A mostly standard-conforming variant of std::vector which preallocates 'Cap' elements
	/// for storage on the stack. By default, this amount is either one or as many as can fit
	/// within the required space, whichever is larger.
	template<class T, size_t Cap = std::max((size_t)1, 2 * sizeof(T*) / sizeof(T))>
	class Vector : public types::TriviallyRelocatable
	{
		struct Heap
		{
			char *data; 
			size_t size;
		};

		union
		{
			StaticVector<T, Cap> _static;
			Heap _heap;
		};

		size_t _capacity;

		bool is_static() const { return _capacity == Cap; }

		void switch_to_heap()
		{
			// TODO: handle alignment?
			auto raw_data = new char[sizeof(T) * _capacity];
			auto data = (T *)raw_data;

			auto size = _static.size();

			dib::uninitialized_relocate_n(_static._begin(), size, data);
			_heap.data = raw_data;
			_heap.size = size;
		}

		void switch_to_stack()
		{
			auto data = _heap.data;
			auto size = _heap.size;

			std::construct_at(&_static);
			_static.relocate_from((T *)data, size);

			delete[] data;
		}

		void reallocate_heap()
		{
			auto old_raw_data = _heap.data;
			auto old_data = (T *)old_raw_data;

			_heap.data = new char[sizeof(T) * _capacity];

			dib::uninitialized_relocate_n(old_data, _heap.size, (T *)_heap.data);
			delete[] old_data;
		}

		void destroy() 
		{
			if (!is_static() && _heap.data != nullptr)
			{
				std::destroy_n((T *)_heap.data, _heap.size);
				delete[] _heap.data;
			}
		}

		void copy(const Vector &other)
		{
			_capacity = other._capacity;

			if (is_static())
			{
				std::construct_at(&_static, other._static);
			}
			else
			{
				_heap.size = other._heap.size;

				_heap.data = new char[sizeof(T) * _capacity];
				std::copy_n((T *)other._heap.data, _heap.size, (T *)_heap.data);
			}
		}

		void move(Vector &&other)
		{
			_capacity = other._capacity;

			if (is_static())
			{
				std::construct_at(&_static, MOVE(other._static));
			}
			else
			{
				_heap.size = other._heap.size;
				_heap.data = other._heap.data;

				other._heap.size = 0;
				other._heap.data = nullptr;
			}
		}

		T *_begin() { return is_static() ? _static._begin() : (T*) _heap.data; }
		const T *_begin() const { return is_static() ? _static._begin() : (const T*) _heap.data; }

		T *_end() { return _begin() + size(); }
		const T *_end() const { return _begin() + size(); }

	public:
		using size_type = size_t;
		using iterator = algorithm::BasicRandomAccessIterator<Vector<T>>;
		using const_iterator = algorithm::BasicRandomAccessIterator<const Vector<T>>;

		Vector() : _static(), _capacity(Cap) {}

		Vector(const Vector &other) { copy(other); }
		Vector(Vector &&other) { move(std::move(other)); }
		
		Vector &operator=(const Vector &other) 
		{
			if (ref_equal(*this, other)) return *this;

			destroy(); 
			copy(other); 
			return *this; 
		}
		Vector &operator=(Vector &&other) 
		{
			if (ref_equal(*this, other)) return *this;

			destroy(); 
			move(std::move(other)); 
			return *this; 
		}

		~Vector() { destroy(); }

		size_t size() const { return is_static() ? _static.size() : _heap.size; }
		size_t capacity() const { return _capacity; }
		bool empty() const { return size() == 0; }

		T *data() { return is_static() ? _static.data() : (T*) _heap.data; }
		const T *data() const { return is_static() ? _static.data() : (const T*) _heap.data; }

		iterator begin() { return iterator(this, 0); }
		const_iterator begin() const { return const_iterator(this, 0); }
		const_iterator cbegin() const { return begin(); }

		iterator end() { return begin() + size(); }
		const_iterator end() const { return begin() + size(); }
		const_iterator cend() const { return end(); }

		T &operator[](size_t n) { return *(begin() + n); }
		const T &operator[](size_t n) const { return *(begin() + n); }

		T &front() { return *begin(); }
		const T &front() const { return *begin(); }

		T &back() { return *(end() - 1); }
		const T &back() const { return *(end() - 1); }

		void push_back(const T &value) { emplace(end(), value); }
		void push_back(T &&value) { emplace(end(), MOVE(value)); }

		void insert(iterator it, const T &value) { emplace(it, value); }
		void insert(iterator it, T &&value) { emplace(it, MOVE(value)); }

		template<class... Args>
		T &emplace_back(Args &&...args) { return emplace(end(), FORWARD(args)...); }

		template<class... Args>
		T &emplace(iterator it, Args &&...args)
		{
			auto sz = size();
			auto idx = it - begin();

			if (is_static() && sz < Cap)
			{
				// Static vector can fit new element //
				_static.emplace(_static.begin() + idx, FORWARD(args)...);
				return *it;
			}

			// NOTE: vector must be heap-based beyond this point! //
			// Handle movement to heap and reallocation //
			if (sz == _capacity)
			{
				auto is_static = this->is_static();
				_capacity *= 2;

				if (is_static) switch_to_heap();
				else reallocate_heap();
			}

			it = begin() + idx;

			// Perform emplacement //
			algorithm::array_emplace(_begin() + it.index(), _end(), FORWARD(args)...);
			_heap.size++;

			// Return reference //
			return *it;
		}

		void pop_back()
		{
			erase(cend() - 1);
		}

		void erase(const_iterator cit)
		{
			// If static, delegate //
			if (is_static())
			{
				_static.erase(_static.cbegin() + cit.index());
				return;
			}

			// Shift elements to cover removed //
			algorithm::array_erase(_begin() + cit.index(), _end());
			_heap.size--;

			// Revert to stack based if small enough //
			if (_heap.size == Cap)
				switch_to_stack();
		}

		auto operator<=>(const Vector &other) const
		{
			return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
		}

		bool operator==(const Vector &other) const
		{
			return std::equal(begin(), end(), other.begin());
		}
	};
}