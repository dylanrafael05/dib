#ifndef __DIB_BIJECTION_H
#define __DIB_BIJECTION_H

#include <unordered_set>
#include "optional.h"
#include "sparse_list.h"
#include "types.h"
#include "raw_memory_utils.h"

namespace dib::structures
{
	template<class Base>
	class HashThroughPointer : Base
	{
	public:
		using Base::Base;

		HashThroughPointer(const Base &base) : Base(base) {}
		HashThroughPointer(Base &&base) : Base(std::move(base)) {}

		decltype(auto) operator()(auto &&value) const
		{
			return Base::operator()(*value);
		}
	};

	template<class Base>
	class EqualsThroughPointer : Base
	{
	public:
		using Base::Base;

		EqualsThroughPointer(const Base &base) : Base(base) {}
		EqualsThroughPointer(Base &&base) : Base(std::move(base)) {}

		decltype(auto) operator()(auto &&left, auto &&right) const
		{
			return Base::operator()(*left, *right);
		}
	};

	template<
		class Left,
		class Right,
		class HashLeft = std::hash<Left>,
		class HashRight = std::hash<Right>,
		class EqualLeft = std::equal_to<>,
		class EqualRight = std::equal_to<>,
		class Allocator = std::allocator<int>
	>
	class Bijection
	{
		constexpr static bool LRDiffer = types::not_cvref_eq<Left, Right>;

		template<class T>
		using AllocR = std::allocator_traits<Allocator>::template rebind_alloc<T>;

		using Pair = std::pair<Left, Right>;
		using PairOpt = dib::Optional<Pair>;
		using PairPtr = PairOpt *;
		using PairList = SparseList<Pair, AllocR<Pair>>;
		using LeftMap = std::unordered_set<Left *, HashThroughPointer<HashLeft>, EqualsThroughPointer<EqualLeft>, AllocR<Left *>>;
		using RightMap = std::unordered_set<Right *, HashThroughPointer<HashRight>, EqualsThroughPointer<EqualRight>, AllocR<Right *>>;

		PairList _pairs;
		LeftMap _left;
		RightMap _right;

		static PairPtr to_pair(Left *l) { return (PairPtr)((char *)l - offsetof(Pair, first) - PairOpt::ValueOffset()); }
		static PairPtr to_pair(Right *r) { return (PairPtr)((char *)r - offsetof(Pair, second) - PairOpt::ValueOffset()); }

		void replace_pointers(PairPtr old_pairs_start)
		{
			for (auto it = _left.begin(); it != _left.end(); it++)
			{
				// SAFETY: changing pointers to remain valid does not change equality or hash, and thus
				//         will not affect the structure of the underlying data.
				mem::mutable_cast(*it) = (Left *)(char *)((uintptr_t)(*it) - (uintptr_t)old_pairs_start + (uintptr_t)_pairs.data());
			}

			for (auto it = _right.begin(); it != _right.end(); it++)
			{
				// SAFETY: changing pointers to remain valid does not change equality or hash, and thus
				//         will not affect the structure of the underlying data.
				mem::mutable_cast(*it) = (Right *)(char *)((uintptr_t)(*it) - (uintptr_t)old_pairs_start + (uintptr_t)_pairs.data());
			}
		}

	public:
		Bijection(
			const HashLeft &hashleft = HashLeft(),
			const HashRight &hashright = HashRight(),
			const EqualLeft &equalsleft = EqualLeft(),
			const EqualRight &equalsright = EqualRight(),
			const Allocator &alloc = Allocator()
		)
			: _pairs(alloc), 
			  _left(4, HashThroughPointer<HashLeft>(hashleft), EqualsThroughPointer<EqualLeft>(equalsleft), alloc), 
			  _right(4, HashThroughPointer<HashRight>(hashright), EqualsThroughPointer<EqualRight>(equalsright), alloc)
		{}

		using value_type = Pair;
		using reference = Pair &;
		using pointer = Pair &;
		using iterator = PairList::const_iterator;
		using const_iterator = PairList::const_iterator;

		// Insertion //
		bool insert(Left &&l, Right &&r)
		{
			if (_left.contains(&l) || _right.contains(&r))
				return false;

			auto old_pairs_start = _pairs.data();
			auto old_pairs_cap = _pairs.full_capacity();

			auto idx = _pairs.emplace(std::move(l), std::move(r));

			if (old_pairs_cap != _pairs.full_capacity())
				replace_pointers(old_pairs_start);

			_left.insert(&_pairs.at(idx).first);
			_right.insert(&_pairs.at(idx).second);

			return true;
		}

		bool insert(const Left &l, const Right &r)
		{
			Left lcopy = l;
			Right rcopy = r;

			return insert(std::move(lcopy), std::move(rcopy));
		}

		// Iteration and collection accessors //
		size_t size() const { return _pairs.size(); }

		iterator begin() const { return _pairs.begin(); }
		iterator end() const { return _pairs.end(); }
		iterator cbegin() const { return _pairs.begin(); }
		iterator cend() const { return _pairs.end(); }

		// Element accessors //
		iterator find_left(const Left &l) const
		{
			auto it = _left.find(const_cast<Left *>(&l));
			if (it == _left.end()) return _pairs.end();
			return _pairs.iterator_from_index(to_pair(*it) - _pairs.data());
		}

		iterator find_right(const Right &r) const
		{
			auto it = _right.find(const_cast<Right *>(&r));
			if (it == _right.end()) return _pairs.end();
			return _pairs.iterator_from_index(to_pair(*it) - _pairs.data());
		}

		iterator find(const Left &v) const requires LRDiffer { return find_left(v); }
		iterator find(const Right &v) const requires LRDiffer { return find_right(v); }

		bool contains_left(const Left &l) const { return find_left(l) != _pairs.end(); }
		bool contains_right(const Right &r) const { return find_right(r) != _pairs.end(); }

		bool contains(const Left &v) const requires LRDiffer { return contains_left(v); }
		bool contains(const Right &v) const requires LRDiffer { return contains_right(v); }

		// Shorthand accessors //
		const Right &get_left(const Left &l) const
		{
			return (*to_pair(*_left.find(l)))->second;
		}

		const Left &get_right(const Right &r) const
		{
			return (*to_pair(*_right.find(r)))->first;
		}

		const Left &get(const Right &v) const requires LRDiffer { return get_right(v); }
		const Right &get(const Left &v) const requires LRDiffer { return get_left(v); }

		const Left &operator[](const Right &v) const requires LRDiffer { return get(v); }
		const Right &operator[](const Left &v) const requires LRDiffer { return get(v); }

		// Erasure and modification //
		void erase(iterator it)
		{
			auto &pair = _pairs.at(it.index());

			_left.erase(&pair.first);
			_right.erase(&pair.second);

			_pairs.free(it.index());
		}
	};
}

#endif