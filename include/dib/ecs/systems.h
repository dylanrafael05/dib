#pragma once

#include <concepts>
#include <vector>
#include <variant>
#include <memory>

#include "dib/record.h"
#include "dib/types.h"
#include "dib/functional.h"
#include "dib/pointers.h"
#include "dib/ecs/systems_fwd.h"
#include "dib/ecs/world.h"

namespace dib::ecs
{
	// System function creation //
	template<class R>
	struct [[=provides_hash]] BasicSystemFn
	{
	public:
		BasicSystemFn(R(*fn)())
			: _fn(fn)
		{}

		R operator()() const
		{
			return _fn();
		}

		size_t get_hash() const { return dib::get_hash(_fn); }

		bool operator==(const BasicSystemFn &) const = default;

	private:
		R(*_fn)();
	};

	using SystemFn = BasicSystemFn<void>;
	using SystemPred = BasicSystemFn<bool>;
	using SystemAction = std::variant<SystemFn, SystemGroup>;

	// System options //
	namespace detail
	{
		struct SystemOption
		{};

		struct SystemRunAfter : SystemOption
		{
			constexpr SystemRunAfter(SystemAction &&key) : key(MOVE(key)) {}
			SystemAction key;
		};
		struct SystemRunBefore : SystemOption
		{
			constexpr SystemRunBefore(SystemAction &&key) : key(MOVE(key)) {}
			SystemAction key;
		};
		struct SystemRunIf : SystemOption
		{
			SystemRunIf(SystemPred &&pred) : pred(MOVE(pred)) {}
			SystemPred pred;
		};
		struct SystemInitWith : SystemOption
		{
			constexpr SystemInitWith(SystemFn &&fn) : fn(MOVE(fn)) {}
			SystemFn fn;
		};
		struct SystemDeinitWith : SystemOption
		{
			constexpr SystemDeinitWith(SystemFn &&fn) : fn(MOVE(fn)) {}
			SystemFn fn;
		};

		template<std::derived_from<SystemOption> LHS, std::derived_from<SystemOption> RHS> 
		struct SystemMergedOption : SystemOption
		{
			LHS left;
			RHS right;

			constexpr SystemMergedOption(LHS &&left, RHS &&right)
				: left(MOVE(left)), right(MOVE(right))
			{}
			constexpr SystemMergedOption(const LHS &left, RHS &&right)
				: left(left), right(MOVE(right))
			{}
			constexpr SystemMergedOption(LHS &&left, const RHS &right)
				: left(MOVE(left)), right(right)
			{}
			constexpr SystemMergedOption(const LHS &left, const RHS &right)
				: left(left), right(right)
			{}
		};

		constexpr auto operator|(std::derived_from<SystemOption> auto &&lhs, std::derived_from<SystemOption> auto &&rhs)
		{
			return SystemMergedOption { FORWARD(lhs), FORWARD(rhs) };
		}
		constexpr auto operator|(std::derived_from<SystemOption> auto &lhs, std::derived_from<SystemOption> auto &&rhs)
		{
			return SystemMergedOption{ FORWARD(lhs), FORWARD(rhs) };
		}
		constexpr auto operator|(std::derived_from<SystemOption> auto &&lhs, std::derived_from<SystemOption> auto &rhs)
		{
			return SystemMergedOption{ FORWARD(lhs), FORWARD(rhs) };
		}
	}

	namespace options
	{
		inline detail::SystemRunAfter after(SystemAction &&action) { return { MOVE(action) }; }
		inline detail::SystemRunBefore before(SystemAction &&action) { return { MOVE(action) }; }
		inline detail::SystemRunIf run_if(SystemPred &&pred) { return { MOVE(pred) }; }
		inline detail::SystemInitWith init_with(SystemFn &&fn) { return { MOVE(fn) }; }
		inline detail::SystemDeinitWith deinit_with(SystemFn &&fn) { return { MOVE(fn) }; }
	}

	// System class and methods //
	struct System
	{
		System(SystemGroup group, auto &&action)
			: group(group), action(FORWARD(action))
		{}

		template<types::IsDerivedFrom<detail::SystemOption> Opt>
		System(SystemGroup group, SystemAction &&action, Opt &&opt)
			: System(group, MOVE(action))
		{
			option(FORWARD(opt));
		}

		void execute(const Systems &scheduler, World &world) const;

		friend struct Systems;

	private:
		enum class ConstructionState : char
		{
			unstarted,
			in_progress,
			completed
		};

		SystemGroup group;
		SystemAction action;
		functional::Multievent<> init;
		functional::Multievent<> deinit;
		functional::Multipredicate<> pred;
		std::vector<SystemAction> order_after;
		std::vector<SystemAction> order_before;

		int order = 0;
		ConstructionState order_state = ConstructionState::unstarted;

		void option(detail::SystemRunIf &&r) { pred.append(MOVE(r.pred)); }
		void option(detail::SystemRunAfter &&r) { order_after.push_back(MOVE(r.key)); }
		void option(detail::SystemRunBefore &&r) { order_before.push_back(MOVE(r.key)); }
		void option(detail::SystemInitWith &&r) { init.append(MOVE(r.fn)); }
		void option(detail::SystemDeinitWith &&r) { deinit.append(MOVE(r.fn)); }

		void option(const detail::SystemRunIf &r) { pred.append(r.pred); }
		void option(const detail::SystemRunAfter &r) { order_after.push_back(r.key); }
		void option(const detail::SystemRunBefore &r) { order_before.push_back(r.key); }
		void option(const detail::SystemInitWith &r) { init.append(r.fn); }
		void option(const detail::SystemDeinitWith &r) { deinit.append(r.fn); }

		template<class LHS, class RHS>
		void option(detail::SystemMergedOption<LHS, RHS> &&r) { option(MOVE(r.left)); option(MOVE(r.right)); }
		template<class LHS, class RHS>
		void option(const detail::SystemMergedOption<LHS, RHS> &r) { option(r.left); option(r.right); }
	};

	// Systems //
	namespace detail
	{
		void initialize_main(Systems &scheduler);

		struct SystemGroupInstance
		{
			friend struct dib::ecs::Systems;

		private:
			std::unordered_map<SystemAction, std::unique_ptr<System>> _system_map;
			std::vector<dib::BorrowedPtr<System>> _system_list;
		};
	}

	struct Systems
	{
		void add(System &&sys);
		void add(const std::initializer_list<System> &systems);

		void build();

		void execute(World &world, SystemGroup group) const;

	private:
		bool _built = false;
		std::unordered_map<SystemGroup, detail::SystemGroupInstance> _groups;
	};
}