#include "dib/ecs/systems.h"
#include "dib/debug.h"
#include "dib/ecs/world.h"

#include <numeric>
#include <ranges>

using namespace dib::ecs;

// Systems //
void System::execute(const Systems &scheduler, World &world) const
{
	if (pred && !pred()) [[unlikely]]
		return;

	init();
	std::visit(
		functional::Overload{
			[&](const SystemFn &fn) { fn(); },
			[&](const SystemGroup &group) { scheduler.execute(world, group); }
		},
		action
	);
	deinit();
}

// SystemScheduler //
void Systems::add(System &&sys)
{
	if (_built)
	{
		RUNTIME_ERROR("Calls to SystemScheduler::add_system() must be performed before calling SystemScheduler::build().");
	}

	auto ptr = std::make_unique<System>(MOVE(sys));
	auto grp = ptr->group;
	auto act = ptr->action;

	auto &grp_inst = _groups[grp];

	grp_inst._system_map.insert({ act, MOVE(ptr) });
	grp_inst._system_list.push_back(grp_inst._system_map.at(act));
}

void Systems::add(const std::initializer_list<System> &systems)
{
	if (_built)
	{
		RUNTIME_ERROR("Calls to SystemScheduler::add_systems() must be performed before calling SystemScheduler::build().");
	}

	for (auto &s_ref : systems)
	{
		auto s = dib::copy(s_ref);
		add(std::move(s));
	}
}

void Systems::build()
{
	// NOTE: this is pretty dang important! do this by 'normalizing' the ordering graph so it only flows in one direction (probably 'after')
	// TODO: use traversal to calculate order instead of this method, since it *can* break down
	//       if a system is inserted between two adjacently ordered (System::order is a difference of one).

	auto calculate_order = [&](detail::SystemGroupInstance &grp, System &sys, auto &&rec) -> void
	{
		if (sys.order_state == System::ConstructionState::in_progress)
		{
			RUNTIME_ERROR("Recursive ordering detected.");
			// TODO: make it so calling .add_system takes in a source_location defaulted to ::current,
			// then report that source location here to make this error less vague!
		}
		else if (sys.order_state == System::ConstructionState::completed)
		{
			return;
		}

		sys.order_state = System::ConstructionState::in_progress;

		auto min = std::numeric_limits<int>::min();
		auto max = std::numeric_limits<int>::max();

		for (auto &after : sys.order_after)
		{
			auto &after_inst = *grp._system_map.at(after);
			rec(grp, after_inst, rec);

			min = std::max(min, after_inst.order);
		}

		for (auto &before : sys.order_before)
		{
			auto &before_inst = *grp._system_map.at(before);
			rec(grp, before_inst, rec);

			max = std::min(max, before_inst.order);
		}

		if (min > max)
		{
			RUNTIME_ERROR("Impossible ordering detected.");
		}

		sys.order = std::midpoint(min, max);
		sys.order_state = System::ConstructionState::completed;
	};

	for (auto &[_, grp] : _groups)
	{
		for (auto &sys : grp._system_list)
		{
			calculate_order(grp, *sys, calculate_order);
		}

		std::ranges::stable_sort(grp._system_list, {}, &System::order);
	}

	_built = true;
}

void Systems::execute(World &world, SystemGroup group_id) const
{
	if (!_built)
	{
		RUNTIME_ERROR("Calls to SystemScheduler::execute() must be performed after calling SystemScheduler::build().");
	}

	if (!_groups.contains(group_id))
		return;

	for (auto &stm : _groups.at(group_id)._system_list)
		stm->execute(*this, world);
}