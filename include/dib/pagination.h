#ifndef __DIB_PAGINATION_H
#define __DIB_PAGINATION_H

#include "vectors.h"
#include "ops.h"
#include <stdint.h>
#include <numeric>
#include <ranges>

namespace dib::pagination
{
	/// <summary>
	/// Checks if a structure models an evaluator of some type K. This is the case when
	/// it publically exposes a method .evaluate(const K &) const which returns an integral type.
	/// </summary>
	template<class T, class K>
	concept evaluator_for = requires(const T &eval, const K &value)
	{
		{ eval.evaluate(value) } -> std::integral;
	};

	/// <summary>
	/// Provides a default evaluation method for the provided type.
	/// This evaluator returns an object of integral type which is the input,
	/// casted from the input, or zero if neither is applicable.
	/// </summary>
	template<class T>
	struct DefaultEvaluator
	{
		auto evaluate(const T &value) const
		{
			if constexpr (std::integral<T>) return value;
			else if constexpr (std::convertible_to<T, int64_t>) return (int64_t)value;
			else return 0;
		}
	};

	static_assert(evaluator_for<DefaultEvaluator<int>, int>, 
		"DefaultEvaluator should be an evaluator.");
	static_assert(evaluator_for<DefaultEvaluator<char *>, char *>,
		"DefaultEvaluator should be an evaluator.");

	/// <summary>
	/// Provides an evaluator interface which called the template
	/// argument with the value to evaluate using std::invoke.
	/// </summary>
	template<auto Fn>
	struct FnEvaluator
	{
		auto evaluate(const auto &value) const
		{
			return std::invoke(Fn, value);
		}
	};

	static_assert(evaluator_for<FnEvaluator<[](int x) { return x; }>, int>,
		"FnEvaluator should be an evaluator.");
	static_assert(evaluator_for<FnEvaluator<[](char *x) { return 0; }>, char *>,
		"FnEvaluator should be an evaluator.");

	/// <summary>
	/// The type which an object of the provided type evaluates to
	/// given the provided evaluator. 
	/// </summary>
	template<class K, evaluator_for<K> T>
	using evaluation_t = decltype(std::declval<const T &>().evaluate(std::declval<const K &>()));

	/// <summary>
	/// A data structure which allows for O(1) lookup for values by their evaluation.
	/// </summary>
	template<class T, evaluator_for<T> Evaluator = DefaultEvaluator<T>>
	class PageVector : private Evaluator
	{
		using eval_t = evaluation_t<T, Evaluator>;
		using in_vec_t = structures::PtrVector<T>;
		using out_vec_t = std::vector<in_vec_t>;
		using in_vec_it = in_vec_t::iterator;

		size_t _count;
		eval_t _page_size;
		out_vec_t _pages;

		template<class Tc>
		class iterator_base
		{
			friend class PageVector<T, Evaluator>;

			using out_iter = out_vec_t::iterator;
			using in_iter = in_vec_t::iterator;

			out_iter _out_it;
			out_vec_t *_out_vec;

			in_iter _in_it;
			in_vec_t *_in_vec;

			iterator_base(out_iter oi, out_vec_t *ov, in_iter ii, in_vec_t *iv)
				: _out_it(oi)
				, _out_vec(ov)
				, _in_it(ii)
				, _in_vec(iv)
			{}

		public:
			iterator_base(iterator_base<const Tc> &other) requires !std::is_const_v<Tc>
				: _out_it(other._out_it)
				, _out_vec(other._out_vec)
				, _in_it(other._in_it)
				, _in_vec(other._in_vec)
			{}

			iterator_base &operator++()
			{
				_in_it++;

				while (_in_it == _in_vec->end())
				{
					_out_it++;
					if (_out_it != _out_vec->end())
					{
						_in_it = _out_it->begin();
						_in_vec = &*_out_it;
					}
					else break;
				}

				return *this;
			}

			iterator_base operator++(int) const
			{
				auto copy = *this;
				copy.operator++();
				return copy;
			}

			iterator_base &operator--()
			{
				while (_in_it == _in_vec->begin())
				{
					if (_out_it != _out_vec->begin())
					{
						_out_it--;

						_in_it = _out_it->end();
						_in_vec = &*_out_it;
					}
					else return *this;
				}

				_in_it--;

				return *this;
			}

			iterator_base operator--(int) const
			{
				auto copy = *this;
				copy.operator--();
				return copy;
			}

			Tc &operator*() { return *_in_it; }
			const Tc &operator*() const { return *_in_it; }

			Tc *operator->() { return &*_in_it; }
			const Tc *operator->() const { return &*_in_it; }

			bool operator==(const iterator_base &other) const = default;
			auto operator<=>(const iterator_base &other) const = default;
		};

		eval_t min_value() const { return empty() ? std::numeric_limits<eval_t>::min() : Evaluator::evaluate(*begin()); }

	public:
		using iterator = iterator_base<T>;
		using const_iterator = iterator_base<const T>;
		using size_type = size_t;
		using value_type = T;

		size_t size() const { return _count; }
		bool empty() const { return _count != 0; }

		/// <summary>
		/// Calculate which page the provided value resides within.
		/// </summary>
		size_t get_page(eval_t value) const
		{
			return (size_t)((value - min_value()) / _page_size);
		}

		auto begin() { return iterator(_pages.begin(), &_pages, empty() ? {} : _pages.front().begin(), empty() ? nullptr : &_pages.front()); }
		auto begin() const { return const_cast<PageVector *>(this)->begin(); }
		auto cbegin() const { return begin(); }

		auto end() { return iterator(_pages.end(), &_pages, {}, nullptr); }
		auto end() const { return const_cast<PageVector *>(this)->end(); }
		auto cend() const { return end(); }

		iterator find(eval_t value)
		{
			if (empty()) return end();

			auto min = Evaluator::evaluate(*begin());
			auto max = Evaluator::evaluate(*(end() - 1));

			if (value < min || value > max) return end();

			auto pagen = get_page(value);

			auto &page = _pages[pagen];
			auto loc = std::lower_bound(page.begin(), page.end(), value, DIB_LMB_1(Evaluator::evaluate(_1) < value));

			return loc;
		}

		const_iterator find(eval_t value) const
		{
			return const_cast<PageVector *>(this)->find(value);
		}

		iterator insert(const T &value) { return emplace(value); }
		iterator insert(T &&value) { return emplace(DIB_MOV(value)); }

		iterator emplace(auto &&...args)
		{
			// TODO: handle adding pages for values outside of current range //

			auto item = T(DIB_FWD(args)...);
			auto value = Evaluator::evaluate(item);

			auto it = find(value);
			auto iidx = it._in_it - it._out_it->begin();

			it._out_it->insert(DIB_MOV(item), it._in_it);
			_count++;

			return it._out_it->begin() + iidx;
		}

		void erase(const_iterator it)
		{
			// TODO: handle removing pages for values at the end of the current range //

			it._out_it->erase(it._in_it);
			_count--;
		}
	};
}

#endif