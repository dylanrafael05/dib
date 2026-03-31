#pragma once

#include <cstddef>
#include <iterator>
#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <type_traits>

#include "dib/types.h"
#include "dib/debug.h"

namespace dib::algorithm
{
	/// Emplace an element at a position within a contiguous array.
	/// This function assumes that there is enough space in the full array
	/// to safely write to *end.
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
		std::construct_at(it, FORWARD(args)...);
	}

	/// Erase an element at a position within a contiguous array.
	/// The final element's data is considered garbage after this function
	/// completes, and must not be destructed.
	template<class T>
	void array_erase(T *it, T *end)
	{
		dib::relocate_n(it + 1, end - it, it);
	}
	
	/// An iterator for a generic contiguous or random-access container. Note that
	/// this iterator is valid only as long as the container is *unmoved*.
	template<class Container>
	class BasicRandomAccessIterator : public types::TriviallyRelocatable
	{
		Container *_c;
		ptrdiff_t _index;
		
		void assert_inbounds() const
		{
			if(_index < 0 || (size_t) _index >= _c->size())
				RUNTIME_ERROR("Index out of bounds");
		}

		using Self = BasicRandomAccessIterator<Container>;

	public:
		using difference_type = ptrdiff_t;
		
		constexpr static bool IsContiguous = requires(Container &c)
		{ 
			{ c.data() } -> types::IsPointer;
		};

		constexpr BasicRandomAccessIterator() : _c(nullptr), _index(0) {}
		constexpr BasicRandomAccessIterator(Container *c, ptrdiff_t index) : _c(c), _index(index) {}

		constexpr Container &container() const { return *_c; }
		constexpr ptrdiff_t index() const { return _index; }

		constexpr operator BasicRandomAccessIterator<const Container>()
		{
			return {_c, _index};
		}

		constexpr auto operator*() const -> decltype(auto)
		{
			assert_inbounds();

			if constexpr(IsContiguous)
			{
				return *(_c->data() + _index);
			}
			else return _c->get(_index);
		}
		
		using reference = std::invoke_result_t<decltype(&Self::operator*), const Self *>;
		using value_type = std::remove_cvref_t<reference>;
		using pointer = std::add_pointer_t<std::remove_reference_t<reference>>;
		using iterator_category = std::random_access_iterator_tag;
		using iterator_concept = std::conditional_t<
			IsContiguous, std::contiguous_iterator_tag,
			std::random_access_iterator_tag>;
			
		constexpr pointer operator->() const requires IsContiguous
		{
			return std::addressof(**this);
		}

		constexpr Self &operator++()
		{
			_index++;
			return *this;
		}

		constexpr Self operator++(int)
		{
			auto tmp = *this;
			operator++();
			return tmp;
		}

		constexpr Self &operator--()
		{
			_index--;
			return *this;
		}
		
		constexpr Self operator--(int)
		{
			auto tmp = *this;
			operator--();
			return tmp;
		}

		constexpr Self &operator+=(difference_type i)
		{
			_index += i;
			return *this;
		}

		constexpr Self operator+(difference_type i) const
		{
			auto res = *this;
			res += i;
			return res;
		}

		constexpr Self &operator-=(difference_type i)
		{
			_index -= i;
			return *this;
		}

		constexpr Self operator-(difference_type i) const
		{
			auto res = *this;
			res -= i;
			return res;
		}

		constexpr difference_type operator-(const Self &other) const
		{
			return _index - other._index;
		}

		constexpr reference operator[](difference_type n) const
		{
			return *(*this + n);
		}

		constexpr bool operator==(const Self &other) const = default;
		constexpr auto operator<=>(const Self &other) const = default;
	};

	template<class Container>
	decltype(auto) operator+(
		typename BasicRandomAccessIterator<Container>::difference_type n, 
		const BasicRandomAccessIterator<Container> &self)
	{
		return self + n;
	}

	static_assert(std::random_access_iterator<
		BasicRandomAccessIterator<std::vector<int>>>, "Should");

	static_assert(std::random_access_iterator<
		BasicRandomAccessIterator<const std::vector<int>>>, "Should");

}