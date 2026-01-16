#pragma once

#include "dib/types.h"

namespace dib::ecs
{
	struct SystemGroup : types::HashProvided
	{
		static SystemGroup create()
		{
			static unsigned int id_counter = 1;

			SystemGroup grp;
			grp.id = id_counter++;
			return grp;
		}

		constexpr static SystemGroup invalid() { return {}; }
		constexpr bool is_invalid() const { return id == 0; }

		size_t get_hash() const
		{
			return dib::get_hash(id);
		}

		auto operator<=>(const SystemGroup &other) const = default;
		bool operator==(const SystemGroup &other) const = default;

		friend struct SystemGroupGenerator;

	private:
		unsigned int id = 0;
	};

	namespace decl
	{
		struct SystemGroup
		{
			consteval SystemGroup() {}
			SystemGroup(const SystemGroup &) = delete;
			SystemGroup(SystemGroup &&) = delete;

			ecs::SystemGroup get()
			{
				if (id.is_invalid())
					id = ecs::SystemGroup::create();

				return id;
			}

			operator ecs::SystemGroup() { return get(); }

		private:
			ecs::SystemGroup id = ecs::SystemGroup::invalid();
		};
	}

	namespace groups
	{
		inline decl::SystemGroup Main;

		inline decl::SystemGroup First;
		inline decl::SystemGroup Last;

		inline decl::SystemGroup PreUpdate;
		inline decl::SystemGroup Update;
		inline decl::SystemGroup PostUpdate;

		inline decl::SystemGroup StartRender;
		inline decl::SystemGroup Render;
		inline decl::SystemGroup EndRender;

		inline decl::SystemGroup OnInit;
		inline decl::SystemGroup OnDeinit;

		inline decl::SystemGroup Start;

		inline decl::SystemGroup HandleStateTransitions;

		namespace detail
		{
			template<class Arg>
			struct Generator
			{
			public:
				SystemGroup get(const Arg &arg)
				{
					if (map.contains(arg)) return map.at(arg);
					else return map[arg] = SystemGroup::create();
				}

				SystemGroup get_or(const Arg &arg, SystemGroup group) const
				{
					if (map.contains(arg)) return map.at(arg);
					else return group;
				}

			private:
				std::unordered_map<Arg, SystemGroup> map;
			};

			template<class Arg>
			Generator<Arg> &OnEnterGenerator()
			{
				static Generator<Arg> gen;
				return gen;
			}

			template<class Arg>
			Generator<Arg> &OnExitGenerator()
			{
				static Generator<Arg> gen;
				return gen;
			}
		}

		template<class State>
		inline decl::SystemGroup OnEnterFallback;
		template<class State>
		SystemGroup OnEnter(const State &state)
		{
			return detail::OnEnterGenerator<State>().get(state);
		}
		template<class State>
		SystemGroup OnEnterOrFallback(const State &state)
		{
			return detail::OnEnterGenerator<State>().get_or(state, OnEnterFallback<State>);
		}

		template<class State>
		inline decl::SystemGroup OnExitFallback;
		template<class State>
		SystemGroup OnExit(const State &state)
		{
			return detail::OnExitGenerator<State>().get(state);
		}
		template<class State>
		SystemGroup OnExitOrFallback(const State &state)
		{
			return detail::OnExitGenerator<State>().get_or(state, OnExitFallback<State>);
		}
	}

	struct Systems;
	struct System;
}