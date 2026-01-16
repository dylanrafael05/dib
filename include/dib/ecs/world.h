#pragma once

#include "dib/ecs/components.h"
#include "dib/ecs/singletons.h"
#include "dib/ecs/world_fwd.h"
#include "dib/ecs/state_machine.h"
#include "dib/resources.h"

namespace dib::ecs
{
	class World :
		private Entities,
		private Singletons,
		private Messages,
		private StateMachine,
		private Commands,
		private resources::Resources
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
		resources::Resources &resources() { return *this; }
		const resources::Resources &resources() const { return *this; }

		// StateMachine //
		template<class T> bool is_in_state(const T &state) const { return StateMachine::current<T>() == state; }
		template<class T> const T &current_state() const { return StateMachine::current<T>(); }
		template<class T> World &init_state(const T &value) { StateMachine::init<T>(value); return *this; }

		// Entities //
		EntityID create_entity() { return Entities::create_entity(); }
		template<class T> T &get_component(EntityID id) { return Entities::get_component<T>(id); }
		template<class T> const T &get_component(EntityID id) const { return Entities::get_component<T>(id); }
		const Archetype &get_archetype(EntityID id) const { return Entities::get_archetype(id); }
		template<class T> bool has_component(EntityID id) const { return Entities::has_component<T>(id); }
		template<class T, class... Args> World &add_component(EntityID id, Args &&...args) { Entities::add_component<T>(id, std::forward<Args>(args)...); return *this; }
		template<class T> World &remove_component(EntityID id) { Entities::remove_component<T>(id); return *this; }
		World &destroy_entity(EntityID id) { Entities::destroy_entity(id); return *this; }
		template<class... Comp> Query<Comp...> query() { return Entities::query<Comp...>(); }

		// Singletons //
		template<class T, class... Args> World &create_singleton(Args &&...args) { Singletons::create<T>(std::forward<Args>(args)...); return *this; }
		template<class T, class... Args> T &get_new_singleton(Args &&...args) { return Singletons::get_new<T>(std::forward<Args>(args)...); }
		template<class T> World &remove_singleton() { Singletons::remove<T>(); return *this; }
		template<class T> T &get_singleton() { return Singletons::get<T>(); }
		template<class T> bool has_singleton() { return Singletons::has<T>(); }

		// Messages //
		template<class T> World &send_message(T &&message) { Messages::send(std::forward<T>(message)); return *this; }
		template<class T> bool has_message() { return Messages::has<T>(); }
		template<class T> std::optional<T> get_message() { return Messages::get<T>(); }
	};
}