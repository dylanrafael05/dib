#ifndef __DIB_VECTORS_H
#define __DIB_VECTORS_H

#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <compare>
#include <variant>

#include "types.h"

namespace dib::algorithm
{
	/// <summary>
	/// Emplace an element at a position within a contiguous array.
	/// This function assumes that there is enough space in the full array
	/// to safely write to *end.
	/// </summary>
	template<class T, class... Args>
	void array_emplace(T *it, T *end, Args &&...args)
	{
		if (it != end)
		{
			// Swap towards end //
			dib::uninitialized_relocate(end - 1, end);
			dib::relocate_n(it, end - 1 - it, it + 1);
		}

		// Emplace at iterator //
		std::construct_at(it, std::forward<Args>(args)...);
	}

	/// <summary>
	/// Erase an element at a position within a contiguous array.
	/// The final element's data is considered garbage after this function
	/// completes, and must not be destructed.
	/// </summary>
	template<class T>
	void array_erase(T *it, T *end)
	{
		dib::relocate_n(it + 1, end - it, it);
	}
}

namespace dib::structures
{
	template<class T, size_t Cap>
	class StaticVector : types::TriviallyRelocatable
	{
		alignas(T) char _data[sizeof(T) * Cap];
		size_t _size;

		void assert_growable() const
		{
			if (_size >= Cap)
				throw std::bad_alloc{};
		}

		void destroy()
		{
			std::destroy_n(begin(), _size);
		}

		void copy(const StaticVector &other)
		{
			_size = other.size();
			std::uninitialized_copy_n(other.begin(), other.size(), begin());
		}

		void move(StaticVector &&other)
		{
			_size = other.size();
			dib::uninitialized_relocate_n(other.begin(), other.size(), begin());
		}

	public:
		using size_type = size_t;
		using iterator = T *;
		using const_iterator = const T *;

		StaticVector() : _size(0) {}
		StaticVector(const StaticVector &other) { copy(other); }
		StaticVector(StaticVector &&other) { move(std::move(other)); }

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
			move(std::move(other));
			return *this;
		}

		~StaticVector() { destroy(); }

		size_type size() const { return _size; }
		bool empty() const { return _size == 0; }
		bool full() const { return size() == Cap; }

		iterator begin() { return (iterator)_data; }
		const_iterator begin() const { return (const_iterator)_data; }
		const_iterator cbegin() const { return (const_iterator)_data; }

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
		void push_back(T &&value) { emplace(end(), std::move(value)); }

		void insert(iterator it, const T &value) { emplace(it, value); }
		void insert(iterator it, T &&value) { emplace(it, std::move(value)); }

		template<class... Args>
		T &emplace_back(Args &&...args) { return emplace(end(), std::forward<Args>(args)...); }

		template<class... Args>
		T &emplace(iterator it, Args &&...args)
		{
			// Grow vector internally //
			assert_growable();

			// Perform emplacement //
			algorithm::array_emplace(it, end(), std::forward<Args>(args)...);
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
			algorithm::array_erase(const_cast<iterator>(cit), const_cast<iterator>(end()));
			_size--;
		}

		/// <summary>
		/// Relocate elements from the provided contiguous array into this vector.
		/// </summary>
		void relocate_from(T *pointer, size_t n)
		{
			// Throw if relocation is not possible //
			if (n > Cap) 
				throw std::bad_alloc{};

			// Relocate elements (note that relocate_n isn't applicable here, since _size != n) //
			std::destroy_n(begin(), _size);
			dib::uninitialized_relocate_n(begin(), pointer, n);
			
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

	/// <summary>
	/// A mostly standard-conforming variant of std::vector which preallocates 'Cap' elements
	/// for storage on the stack. By default, this amount is either one or as many as can fit
	/// within the required space, whichever is larger.
	/// </summary>
	template<class T, size_t Cap = std::max((size_t)1, 2 * sizeof(T*) / sizeof(T))>
	class SvoVector : types::TriviallyRelocatable
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

			dib::uninitialized_relocate_n(_static.begin(), size, data);
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

			dib::uninitialized_relocate_n(old_data, _heap.size, (iterator)_heap.data);
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

		void copy(const SvoVector &other)
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
				std::copy_n((iterator)other._heap.data, _heap.size, (iterator)_heap.data);
			}
		}

		void move(SvoVector &&other)
		{
			_capacity = other._capacity;

			if (is_static())
			{
				std::construct_at(&_static, std::move(other._static));
			}
			else
			{
				_heap.size = other._heap.size;
				_heap.data = other._heap.data;

				other._heap.size = 0;
				other._heap.data = nullptr;
			}
		}

	public:
		using size_type = size_t;
		using iterator = T *;
		using const_iterator = const T *;

		SvoVector() : _static(), _capacity(Cap) {}

		SvoVector(const SvoVector &other) { copy(other); }
		SvoVector(SvoVector &&other) { move(std::move(other)); }
		
		SvoVector &operator=(const SvoVector &other) 
		{
			if (ref_equal(*this, other)) return *this;

			destroy(); 
			copy(other); 
			return *this; 
		}
		SvoVector &operator=(SvoVector &&other) 
		{
			if (ref_equal(*this, other)) return *this;

			destroy(); 
			move(std::move(other)); 
			return *this; 
		}

		~SvoVector() { destroy(); }

		size_t size() const { return is_static() ? _static.size() : _heap.size; }
		size_t capacity() const { return _capacity; }
		bool empty() const { return size() == 0; }

		iterator begin() { return is_static() ? _static.begin() : (iterator)_heap.data; }
		const_iterator begin() const { return is_static() ? _static.begin() : (const_iterator)_heap.data; }
		const_iterator cbegin() const { return begin(); }

		iterator end() { return is_static() ? _static.end() : (iterator)_heap.data + _heap.size; }
		const_iterator end() const { return is_static() ? _static.end() : (const_iterator)_heap.data + _heap.size; }
		const_iterator cend() const { return end(); }

		T &operator[](size_t n) { return *(begin() + n); }
		const T &operator[](size_t n) const { return *(begin() + n); }

		T &front() { return *begin(); }
		const T &front() const { return *begin(); }

		T &back() { return *(end() - 1); }
		const T &back() const { return *(end() - 1); }

		void push_back(const T &value) { emplace(end(), value); }
		void push_back(T &&value) { emplace(end(), std::move(value)); }

		void insert(iterator it, const T &value) { emplace(it, value); }
		void insert(iterator it, T &&value) { emplace(it, std::move(value)); }

		template<class... Args>
		T &emplace_back(Args &&...args) { return emplace(end(), std::forward<Args>(args)...); }

		template<class... Args>
		T &emplace(iterator it, Args &&...args)
		{
			auto sz = size();
			auto idx = it - begin();

			if (is_static() && sz <= Cap)
			{
				// Static vector can fit new element //
				_static.emplace(it, std::forward<Args>(args)...);
				return *it;
			}

			// NOTE: vector must be heap-based beyond this point! //
			// Handle movement to heap and reallocation //
			if (sz == _capacity)
			{
				_capacity *= 2;

				if (is_static()) switch_to_heap();
				else reallocate_heap();
			}

			it = begin() + idx;

			// Perform emplacement //
			algorithm::array_emplace(it, end(), std::forward<Args>(args)...);
			_heap.size++;

			// Return reference //
			return *it;
		}

		void pop_back()
		{
			erase(end() - 1);
		}

		void erase(const_iterator cit)
		{
			// If static, delegate //
			if (is_static())
			{
				_static.erase(cit);
				return;
			}

			// Shift elements to cover removed //
			algorithm::array_erase(const_cast<iterator>(cit), const_cast<iterator>(end()));
			_heap.size--;

			// Revert to stack based if small enough //
			if (_heap.size == Cap)
				switch_to_stack();
		}

		auto operator<=>(const SvoVector &other) const
		{
			return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
		}

		bool operator==(const SvoVector &other) const
		{
			return std::equal(begin(), end(), other.begin());
		}
	};

	/// <summary>
	/// A vector whose size on the stack is the same as that of one pointer.
	/// To accomplish this, size and capacity data are stored on the heap alongside
	/// elements, unless the vector is empty, in which case the underlying pointer
	/// is null (thus there is still no allocation for empty vectors), in which case 
	/// size and capacity are inferred to be zero.
	/// </summary>
	template<class T>
	class PtrVector
	{
		char *_data;

		struct Header
		{
			size_t size;
			size_t capacity;
		};

		constexpr static size_t HeaderSize = (sizeof(Header) + sizeof(T) - 1) / sizeof(T);

		Header &header() { return *reinterpret_cast<Header*>(_data); }
		const Header &header() const { return *reinterpret_cast<const Header*>(_data); }

		void destroy()
		{
			if (!empty())
			{
				std::destroy_n(data(), size());
				delete[] _data;
			}
		}

		void copy(const PtrVector<T> &other)
		{
			if (other.empty())
			{
				_data = nullptr;
				return;
			}

			_data = new char[sizeof(T) * (HeaderSize + other.size())];
			header().size = other.size();
			header().capacity = other.size();

			std::uninitialized_copy_n(other.data(), size(), data());
		}

		void move(PtrVector<T> &&other)
		{
			_data = other._data;
			other._data = nullptr;
		}

		void grow()
		{
			if (size() == 0)
			{
				_data = new char[sizeof(T) * (HeaderSize + 4)];
				header().size = 0;
				header().capacity = 4;
			}
			else if (size() == capacity())
			{
				auto old_data = data();
				auto sz = size();
				auto cap = capacity();

				cap *= 2;
				_data = new char[sizeof(T) * (HeaderSize + cap)];

				dib::uninitialized_relocate_n(old_data, sz, data());

				header().size = sz;
				header().capacity = cap;
			}

			header().size++;
		}

	public:
		using size_type = size_t;
		using iterator = T *;
		using const_iterator = const T *;

		PtrVector() : _data(nullptr)
		{}

		PtrVector(const PtrVector &other) { copy(other); }
		PtrVector(PtrVector &&other) { move(DIB_MOV(other)); }

		PtrVector &operator==(const PtrVector &other)
		{
			if (ref_equal(*this, other))
				return *this;

			destroy();
			copy(other);

			return *this;
		}

		PtrVector &operator==(PtrVector &&other)
		{
			if (ref_equal(*this, other))
				return *this;

			destroy();
			move(DIB_MOV(other));

			return *this;
		}

		bool empty() const { return _data == nullptr || header().size == 0; }

		size_type size() const { return !_data ? 0 : header().size; }
		size_type capacity() const { return !_data ? 0 : header().capacity; }

		T *data() { return _data ? (T *)_data + HeaderSize : nullptr; }
		const T *data() const { return _data ? (const T *)_data + HeaderSize : nullptr; }

		T &operator[](size_t index) { return data()[index]; }
		const T &operator[](size_t index) const { return data()[index]; }

		iterator begin() { return data(); }
		const_iterator begin() const { return data() }
		const_iterator cbegin() const { return data(); }

		iterator end() { return data() + size(); }
		const_iterator end() const { return data() + size(); }
		const_iterator cend() const { return data() + size(); }

		T &front() { return *begin(); }
		const T &front() const { return *begin(); }

		T &back() { return *(end() - 1); }
		const T &back() const { return *(end() - 1); }

		void push_back(const T &value) { emplace(end(), value); }
		void push_back(T &&value) { emplace(end(), std::move(value)); }

		void insert(iterator it, const T &value) { emplace(it, value); }
		void insert(iterator it, T &&value) { emplace(it, std::move(value)); }

		template<class... Args>
		T &emplace_back(Args &&...args) { return emplace(end(), std::forward<Args>(args)...); }

		template<class... Args>
		T &emplace(iterator it, Args &&...args)
		{
			auto idx = it - begin();
			grow();

			it = begin() + idx;
			dib::algorithm::array_emplace(it, end(), DIB_FWD(args)...);

			return *it;
		}

		void pop_back()
		{
			erase(end() - 1);
		}

		void erase(const_iterator it)
		{
			dib::algorithm::array_erase(const_cast<iterator>(it), end());
			header().size--;
		}

		auto operator<=>(const PtrVector &other) const
		{
			return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
		}

		bool operator==(const PtrVector &other) const
		{
			return std::equal(begin(), end(), other.begin());
		}
	};

	static_assert(
		sizeof(PtrVector<int>) == sizeof(int *), 
		"PtrVectors must have the same size as their corresponding pointer type");

	static_assert(
		sizeof(PtrVector<char *>) == sizeof(char **),
		"PtrVectors must have the same size as their corresponding pointer type");
}

#endif 