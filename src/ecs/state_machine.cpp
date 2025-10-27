#include "dib/ecs/state_machine.h"
#include "dib/ecs/systems.h"
#include "dib/ecs/world.h"

using namespace dib::ecs;

void StateMachine::update(const Systems &scheduler)
{
	while (!_scheduled_transitions.empty())
	{
		run_transition(scheduler, _scheduled_transitions.front());
		_scheduled_transitions.pop();
	}
}

void StateMachine::run_transition(const Systems &scheduler, const StateTransition &transition)
{
	auto &state = _current_state.at(transition.value().type());

	scheduler.execute(*world, state.group);
	state.value = transition.value();
	scheduler.execute(*world, transition.enter_group());

	state.group = transition.exit_group();
}