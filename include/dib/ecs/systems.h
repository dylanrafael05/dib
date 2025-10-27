#ifndef __ECS_DIB_SYSTEMS_H
#define __ECS_DIB_SYSTEMS_H

#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <concepts>
#include <vector>
#include <functional>
#include <variant>
#include <memory>

#include "dib/types.h"
#include "dib/function_util.h"
#include "dib/pointers.h"
#include "dib/raw_memory.h"

#include "dib/ecs/systems_fwd.h"
#include "dib/ecs/world.h"

namespace dib::ecs
{
	// System function creation //
	namespace detail
	{
		template<class T> struct RetrieveQueryType {};
		template<class... Comp> struct RetrieveQueryType<Query<Comp...>>
		{
			static Query<Comp...> get(World &world) { return world.query<Comp...>(); }
		};

		template<class T> T retrieve_system_arguments(const Systems &scheduler, World &world)
		{
			using T_ = std::remove_cvref_t<T>;
			#define is(...) (std::is_same_v<T_, __VA_ARGS__>)

			if constexpr (false);
			else if constexpr is(World) return world;
			else if constexpr is(Commands) return world.commands();
			else if constexpr is(Entities) return world.entities();
			else if constexpr is(Singletons) return world.singletons();
			else if constexpr is(Messages) return world.messages();
			else if constexpr is(StateMachine) return world.state_machine();
			else if constexpr is(Systems) return scheduler;
			else if constexpr is(resources::Resources) return world.resources();
			else if constexpr (is_query<T_>) return RetrieveQueryType<T_>::get(world);
			else return world.get_singleton<T_>();

			#undef is
		}

		template<class T>
		constexpr bool is_valid_system_argument = std::is_lvalue_reference_v<T> ^ ecs::is_query<T>;
	}

	template<class R>
	struct BasicSystemFn : types::HashProvided
	{
	public:
		template<class... Args> 
			requires (detail::is_valid_system_argument<Args> && ...)
		BasicSystemFn(R(&fn)(Args...))
		{
			_fn = (void(*)()) &fn;
			_caller = [](void(*fn)(), const Systems &scheduler, World &world)
			{
				auto fnr = (R(*)(Args...)) fn;
				return fnr(detail::retrieve_system_arguments<Args>(scheduler, world)...);
			};
		}

		R operator()(const Systems &scheduler, World &world) const
		{
			return _caller(_fn, scheduler, world);
		}

		size_t get_hash() const { return dib::get_hash(_fn); }

		bool operator==(const BasicSystemFn &) const = default;

	private:
		void(*_fn)();
		R(*_caller)(void(*)(), const Systems &, World &);
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
			constexpr SystemRunAfter(SystemAction &&key) : key(std::move(key)) {}
			SystemAction key;
		};
		struct SystemRunBefore : SystemOption
		{
			constexpr SystemRunBefore(SystemAction &&key) : key(std::move(key)) {}
			SystemAction key;
		};
		struct SystemRunIf : SystemOption
		{
			SystemRunIf(SystemPred &&pred) : pred(std::move(pred)) {}
			SystemPred pred;
		};
		struct SystemInitWith : SystemOption
		{
			constexpr SystemInitWith(SystemFn &&fn) : fn(std::move(fn)) {}
			SystemFn fn;
		};
		struct SystemDeinitWith : SystemOption
		{
			constexpr SystemDeinitWith(SystemFn &&fn) : fn(std::move(fn)) {}
			SystemFn fn;
		};

		template<std::derived_from<SystemOption> LHS, std::derived_from<SystemOption> RHS> 
		struct SystemMergedOption : SystemOption
		{
			LHS left;
			RHS right;

			constexpr SystemMergedOption(LHS &&left, RHS &&right)
				: left(std::move(left)), right(std::move(right))
			{}
			constexpr SystemMergedOption(const LHS &left, RHS &&right)
				: left(left), right(std::move(right))
			{}
			constexpr SystemMergedOption(LHS &&left, const RHS &right)
				: left(std::move(left)), right(right)
			{}
			constexpr SystemMergedOption(const LHS &left, const RHS &right)
				: left(left), right(right)
			{}
		};

		constexpr auto operator|(std::derived_from<SystemOption> auto &&lhs, std::derived_from<SystemOption> auto &&rhs)
		{
			return SystemMergedOption { std::forward<decltype(lhs)>(lhs), std::forward<decltype(rhs)>(rhs) };
		}
		constexpr auto operator|(std::derived_from<SystemOption> auto &lhs, std::derived_from<SystemOption> auto &&rhs)
		{
			return SystemMergedOption{ std::forward<decltype(lhs)>(lhs), std::forward<decltype(rhs)>(rhs) };
		}
		constexpr auto operator|(std::derived_from<SystemOption> auto &&lhs, std::derived_from<SystemOption> auto &rhs)
		{
			return SystemMergedOption{ std::forward<decltype(lhs)>(lhs), std::forward<decltype(rhs)>(rhs) };
		}
	}

	namespace options
	{
		inline detail::SystemRunAfter after(SystemAction &&action) { return { std::move(action) }; }
		inline detail::SystemRunBefore before(SystemAction &&action) { return { std::move(action) }; }
		inline detail::SystemRunIf run_if(SystemPred &&pred) { return { std::move(pred) }; }
		inline detail::SystemInitWith init_with(SystemFn &&fn) { return { std::move(fn) }; }
		inline detail::SystemDeinitWith deinit_with(SystemFn &&fn) { return { std::move(fn) }; }
	}

	// System class and methods //
	struct System
	{
		System() = default;

		System(SystemGroup group, SystemAction &&action)
			: group(group), action(std::move(action))
		{}

		template<std::derived_from<detail::SystemOption> Opt>
		System(SystemGroup group, SystemAction &&action, Opt &&opt)
			: System(group, std::move(action))
		{
			option(std::forward<Opt>(opt));
		}

		template<std::derived_from<detail::SystemOption> Opt>
		System(SystemGroup group, SystemAction &&action, const Opt &opt)
			: System(group, std::move(action))
		{
			option(opt);
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
		functional::Multievent<const Systems &, World &> init;
		functional::Multievent<const Systems &, World &> deinit;
		functional::Multipredicate<const Systems &, World &> pred;
		std::vector<SystemAction> order_after;
		std::vector<SystemAction> order_before;

		int order = 0;
		ConstructionState order_state = ConstructionState::unstarted;

		void option(detail::SystemRunIf &&r) { pred.append(std::move(r.pred)); }
		void option(detail::SystemRunAfter &&r) { order_after.push_back(std::move(r.key)); }
		void option(detail::SystemRunBefore &&r) { order_before.push_back(std::move(r.key)); }
		void option(detail::SystemInitWith &&r) { init.append(std::move(r.fn)); }
		void option(detail::SystemDeinitWith &&r) { deinit.append(std::move(r.fn)); }

		void option(const detail::SystemRunIf &r) { pred.append(r.pred); }
		void option(const detail::SystemRunAfter &r) { order_after.push_back(r.key); }
		void option(const detail::SystemRunBefore &r) { order_before.push_back(r.key); }
		void option(const detail::SystemInitWith &r) { init.append(r.fn); }
		void option(const detail::SystemDeinitWith &r) { deinit.append(r.fn); }

		template<class LHS, class RHS>
		void option(detail::SystemMergedOption<LHS, RHS> &&r) { option(std::move(r.left)); option(std::move(r.right)); }
		template<class LHS, class RHS>
		void option(const detail::SystemMergedOption<LHS, RHS> &r) { option(r.left); option(r.right); }
	};

	// Systems //
	namespace detail
	{
		void initialize_main(Systems &scheduler);

		struct SystemGroupInstance
		{
			friend struct Systems;

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

		void execute(World &world, SystemGroup group_id) const;

	private:
		bool _built = false;
		std::unordered_map<SystemGroup, detail::SystemGroupInstance> _groups;
	};
}

#endif