#pragma once

#include <any>
#include <unordered_map>
#include <queue>
#include <typeindex>

#include "dib/ecs/world_fwd.h"
#include "dib/ecs/systems_fwd.h"
#include "dib/debug.h"
#include "dib/types.h"

namespace dib::ecs
{
	struct StateTransition
	{
		const std::any &value() const { return _value; }
		SystemGroup enter_group() const { return _enter_group; }
		SystemGroup exit_group() const { return _exit_group; }

		template<class State>
		static StateTransition create(const State &state)
		{
			StateTransition result;
			result._value = state;
			result._enter_group = groups::OnEnterOrFallback(state);
			result._exit_group = groups::OnExitOrFallback(state);
			return result;
		}

	private:
		std::any _value;
		SystemGroup _enter_group;
		SystemGroup _exit_group;
	};

	struct StateMachine
	{
		StateMachine(World *world) : world(world) {}

		StateMachine() = delete;
		StateMachine(const StateMachine &) = delete;
		StateMachine(StateMachine &&) = delete;

		template<class T>
		void init(const T &value)
		{
			if (_current_state.contains(typeid(T)))
			{
				RUNTIME_ERROR(std::format("Attempt to initialize a state of type {} twice.", types::typedesc<T>.name()));
			}

			auto &state = _current_state[typeid(T)];
			state.value = value;
			state.group = groups::OnExitOrFallback(value);
		}

		template<class T>
		const T &current() const
		{
			return *std::any_cast<T>(&_current_state.at(typeid(T)).value);
		}

		template<class T>
		bool is_in(const T &state)
		{
			return current<T>() == state;
		}

		template<class State>
		void change_to(const State &state)
		{
			_scheduled_transitions.push(StateTransition::create(state));
		}

		void update(const Systems &scheduler);

	private:
		void run_transition(const Systems &scheduler, const StateTransition &transition);

		struct State
		{
			std::any value;
			SystemGroup group;
		};

		std::unordered_map<std::type_index, State> _current_state;
		std::queue<StateTransition> _scheduled_transitions;
		World *world;
	};

	template<auto... States>
	bool is_in_state(StateMachine &state)
	{
		return ((state.current<decltype(States)>() == States) && ...);
	}
}