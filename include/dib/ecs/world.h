#pragma once

#include "dib/ecs/entities.h"
#include "dib/ecs/singletons.h"
#include "dib/ecs/messages.h"
#include "dib/ecs/world_fwd.h"
#include "dib/ecs/state_machine.h"
#include "dib/resources/resources.h"

namespace dib::ecs
{
	// TODO: what should we do with this class? consider reorganization.
	
	class World :
		public Entities,
		private Singletons,
		private Messages,
		private StateMachine,
		private Commands,
		private res::Resources
	{
	public:
		World() 
			: Entities(), Singletons(), Messages(), StateMachine(this), Commands(this) {}
		World(const World &) = delete;
		World(World &&) = delete;

		// Base-class getters //
		Entities &entities() { return *this; }
		const Entities &entities() const { return *this; }
		Singletons &singletons() { return *this; }
		const Singletons &singletons() const { return *this; }
		Messages &messages() { return *this; }
		const Messages &messages() const { return *this; }
		StateMachine &state_machine() { return *this; }
		const StateMachine &state_machine() const { return *this; }
		Commands &commands() { return *this; }
		const Commands &commands() const { return *this; }
		res::Resources &resources() { return *this; }
		const res::Resources &resources() const { return *this; }

		// StateMachine //
		template<class T> bool is_in_state(const T &state) const { return StateMachine::current<T>() == state; }
		template<class T> const T &current_state() const { return StateMachine::current<T>(); }
		template<class T> World &init_state(const T &value) { StateMachine::init<T>(value); return *this; }

		// Singletons //
		template<class T, class... Args> World &create_singleton(Args &&...args) { Singletons::create<T>(FORWARD(args)...); return *this; }
		template<class T, class... Args> T &get_new_singleton(Args &&...args) { return Singletons::get_new<T>(FORWARD(args)...); }
		template<class T> World &remove_singleton() { Singletons::remove<T>(); return *this; }
		template<class T> T &get_singleton() { return Singletons::get<T>(); }
		template<class T> bool has_singleton() { return Singletons::has<T>(); }

		// Messages //
		template<class T> World &send_message(T &&message) { Messages::send(FORWARD(message)); return *this; }
		template<class T> bool has_message() { return Messages::has<T>(); }
		template<class T> std::optional<T> get_message() { return Messages::get<T>(); }
	};
}