#pragma once 

#include <concepts>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <stdint.h>

#include "dib/functional.h"
#include "dib/metafunction.h"
#include "dib/metautils.h"
#include "dib/record.h"
#include "dib/sparse_bitset.h"
#include "dib/raw_memory.h"
#include "dib/thread_pool.h"
#include "dib/types.h"
#include "dib/newtype.h"
#include "dib/vector.h"
#include "dib/reflect.h"

namespace dib::ecs
{
    class Entities;
    class Commands;
}

namespace dib
{
    ecs::Entities &entities();
    ecs::Commands &commands();
}

namespace dib::ecs
{
    constexpr static uint64_t INVALID_ID = 0xFFFFFFFFFF;

    /// Mark a type as a component
    struct Component {};
    constexpr Component component;

    template<class T>
    concept IsComponent = AnnotatedWith<T, Component>;

    /// An ID associated with each component type.
    struct ComponentID : public dib::types::Newtype<uint32_t>
    {
        refl::Type type() const;
    };

    class Archetype;

    /// An ID associated with each entity. Comprised of an index and version.
    struct EntityID
    {
        uint64_t index : 40 = 0;
        uint64_t version : 24 = 0;

        EntityID(uint64_t index, uint64_t version)
            : index(index), version(version)
        {}
        EntityID()
            : index(INVALID_ID), version(0)
        {}

        bool is_valid() const;
        bool is_alive() const;
        bool is_invalid() const;

        constexpr bool operator==(const EntityID &) const = default;

        /// Get the archetype of this entity
        const Archetype &get_archetype() const;

        /// Check if the given entity has the provided component type.
        template<IsComponent T>
        bool has_component() const;
        /// Check if the given entity has the provided component type.
        bool has_component(ComponentID cid) const;

        /// Get a reference to the provided component type.
        template<IsComponent T>
        T &get_component() const;
        /// Get a reference to the provided component type.
        void *get_component_raw(ComponentID cid) const;

        static const EntityID invalid;
    };
    
    /// An ID associated with a categorization.
    class CategorizationID : public types::Newtype<size_t> 
    {};

    /// Implementation details for component id mapping below
    namespace detail
    {
        /// Get the component id of a provided type descriptor
        ComponentID component_id(refl::Type desc);

        /// In this ECS architecture, entities are grouped by 'archetype'.
        /// An entity's 'archetype' is the set of components which comprise it.
        /// The components for each entity are stored in densely packed ErasedVec`s
        /// grouped by entity archetype.

        /// The storage for all entity data within one archetype.
        struct ArchetypeStorage;

        /// An ID used to identify archetypes and their corresponding entity stores.
        using ArchetypeID = size_t;

        /// An ID used to identify entity grouping schemes.
        class CategorizationID : public types::Newtype<size_t> {};
        class CategoryID       : public types::Newtype<size_t> {};

        /// An 'edge' connecting two archetypes; the relation from some archetype
        /// to another, given that the two differ by exactly one component. 
        ///
        /// When adding that component type, the resulting archetype is represented in on_add,
        /// and when removing it, the result is represented in on_remove.
        struct ArchetypeEdge
        {
            ArchetypeID on_add = static_cast<ArchetypeID>(-1);
            ArchetypeID on_remove = static_cast<ArchetypeID>(-1);
        };
    }
    
    /// Get the component id of a provided component type
    template<IsComponent T>
    inline ComponentID component_id() 
    {
        return detail::component_id(refl::typeof<T>);
    }

    /// An entity's archetype
    class [[=provides_hash]] Archetype : public types::TriviallyRelocatable
    {
        dib::structures::SparseBitset _bits;

        Archetype(dib::structures::SparseBitset &&bits);

    public:
        Archetype() = default;

        template<class... Comp>
        static Archetype of_components()
        {
            return {
                dib::structures::SparseBitset::from_bits({component_id<Comp>().value()...})
            };
        }

        static Archetype from_bits(dib::structures::SparseBitset &&bits);

        const dib::structures::SparseBitset &bits() const;

        bool has_component(ComponentID id) const;
        bool has_component(refl::Type id) const;

        template<class Comp>
        bool has_component() const { return has_component(component_id<Comp>()); }

        bool is_subset_of(const Archetype &other) const;

        size_t get_hash() const { return _bits.get_hash(); }
        bool operator==(const Archetype &other) const { return _bits == other._bits; }
    };

    /// Get the representation of the provided archetype in-memory.
    /// There is exactly one archetype allocated for each *ordered* set of types
    /// provided to this function.
    template<IsComponent... Comp>
    const Archetype &archetype()
    {
        static Archetype result = Archetype::of_components<Comp...>();
        return result;
    }
    
    namespace detail
    {
        struct EntityCategorization
        {
            ComponentID associated_component;
            dib::functional::Function<CategoryID(EntityCategorization *, void *)> key_finder;
            dib::structures::ErasedVec all_categories;
            dib::structures::Vector<size_t> categories_order;

            // how to store such that ids are properly ordered
        };
        
        using EntityKeyFinder = decltype(EntityCategorization::key_finder);

        struct ArchetypeCategorizationStorage
        {
            CategorizationID id;
            std::unordered_map<CategoryID, std::vector<bool>> key_mappings;
        };

        struct ArchetypeStorage
        {
            /// The archetype (and associated archetype storage index) of this storage unit
            Archetype archetype;
            ArchetypeID index;

            /// The map of all component stores by their associated component type
            std::unordered_map<ComponentID, dib::structures::ErasedVec> component_stores;
            std::unordered_map<CategorizationID, ArchetypeCategorizationStorage> grouping_stores;

            /// A list of the entities stored within this archetype storage, as well as which
            /// indices currently refer to a 'free' entity
            std::vector<EntityID> entity_ids;
            std::vector<size_t> free_indices;
            size_t capacity;

            std::unordered_map<ComponentID, ArchetypeEdge> related_archetypes;

            ArchetypeStorage();
            ArchetypeStorage(ArchetypeStorage &&other);

            ~ArchetypeStorage();

            void create_storage_for(ComponentID type);
        };

        /// The information stored in the Entities class about each entity;
        /// the entity's archetype, its index within that archetype, and the
        /// version that the entity is given.
        struct EntityEntry
        {
            ArchetypeID archetype;
            size_t index;

            uint64_t version;
        };
    }

    /// Forward declare all the content related to queries.
    class BasicQuery;
    template<class ...Components> class Query;
    class Commands;

    /// The 'meat' of the ECS system; a class responsible for all
    /// entity and component interactions.
    class Entities
    {
        // The threading scheduler should probably be moved upwards?
        threading::ThreadScheduler scheduler;

        /// The list of entity entries, indexed by entity index.
        std::vector<detail::EntityEntry> entity_map;
        std::vector<uint64_t> free_ids;
        
        /// The list of all entity groupings. An entity grouping
        /// must last as long as the ecs system.
        std::vector<detail::EntityCategorization> groupings_map;

        /// The list of archetype stores, indexed by archetype id.
        /// Mappings from archetype to id and from query archetype to
        /// vector of id are provided.
        ///
        /// It is important to note that the first archetype (id 0)
        /// will always be the 'empty' archetype.
        std::vector<detail::ArchetypeStorage> archetype_stores;
        std::unordered_map<Archetype, detail::ArchetypeID> archetype_map;
        std::unordered_map<Archetype, std::vector<detail::ArchetypeID>> query_map;
        
        //! UNIMPLEMENTED !//
        ////std::unordered_multimap<ComponentID, detail::CategorizationID> categorization_map;

        //! UNIMPLEMENTED !//
        /// Register a *new* grouping acting over the provided component and with the provided
        /// key-finder. Note that all existing archetypes will have to be modified to ensure that
        /// any for which the grouping apply are updated.
        ////detail::CategorizationID register_categorization(ComponentID id, detail::EntityKeyFinder key_finder);

        /// Register a *new* archetype storage with the entities system.
        /// Returns the ID now associated with that archetype.
        detail::ArchetypeID register_archetype(detail::ArchetypeStorage &&in);

        /// Register a *new* query archetype with the entities system.
        void register_query(const Archetype &in);

        /// Modify the provided archetype (by id) by either adding or removing the
        /// provided component type, updating all internal storage relating to archetypes
        /// and caching if possible.
        detail::ArchetypeStorage &add_or_remove_from_archetype(ComponentID cid, detail::ArchetypeID archetype, bool add);

        /// Allocate space for a new, empty entity.
        uint64_t alloc_entity();
        /// Allocate space within the provided archetype store for a new entity.
        uint64_t alloc_in_arch(detail::ArchetypeStorage &storage);
        /// Free the provided entity.
        void dealloc_entity(uint64_t index);

        /// Assert that the provided entity exists.
        void assert_exists(EntityID id) const;
        
        /// Assert that the provided entity has the provided component.
        void assert_has_component(EntityID id, ComponentID cid) const;

        // TODO: implement groupings!

    public:
        Entities();

        Entities(const Entities&) = delete;
        Entities(Entities &&) = delete;

        threading::ThreadScheduler &thread_scheduler() { return scheduler; }

        /// Check if the provided entity is alive and well.
        bool is_alive(EntityID id) const;

        //! UNIMPLEMENTED !//
        //// template<NotComponentPack Component, types::IsValue Key>
        ////     requires (
        ////         types::IsEqualityComparable<Key> && 
        ////         types::IsThreeWayComparable<Key> && 
        ////         types::IsHashable<Key>
        ////     )
        //// CategorizationID create_categorization(functional::fn<Key(const Component &)> key_getter)
        //// {
        ////     register_categorization(
        ////         component_id<Component>(), 
        ////         [&](detail::EntityCategorization *cat, void *component)
        ////         {
        ////             auto key = key_getter(*(const Component *)component);
        ////             auto key_ref = any::AnyRef(key);
        ////
        ////             auto it = std::lower_bound(
        ////                 cat->all_categories_ordered.begin(), 
        ////                 cat->all_categories_ordered.end(),
        ////                 key_ref);
        ////
        ////             if(*it != key_ref)
        ////             {
        ////                 RUNTIME_ERROR("Could not find requested key");
        ////             }
        ////         }
        ////     );
        //// }

        /// Create a new, empty entity.
        EntityID create_entity();
        
        /// Create a new, uninitialized entity of the provided archetype.
        EntityID create_uninitialized_entity(const Archetype &archetype);

        template<IsComponent... Components>
        EntityID create_entity(Components &&...components)
        {
            auto &archetype = dib::ecs::archetype<Components...>();
            auto entity = create_uninitialized_entity(archetype);

            ([&]
            {
                using Component = Components;
                dib::mem::Forgotten<Component> cons_comp(MOVE(components));

                auto &component = entity.template get_component<Component>();
                dib::uninitialized_relocate((Component *) &cons_comp, &component);

            }(), ...); 

            return entity;
        }

        /// Add a component to the provided entity.
        template<IsComponent T> requires types::IsValue<T>
        void add_component(EntityID id, T &&component)
        {
            dib::mem::Forgotten<T> value(MOVE(component));

            add_component_raw(id, component_id<T>(), (void*) &value);
        }
        /// Add a new component to the provided entity. The component structure must be
        /// located at 'value' and must have type of 'cid'.
        void add_component_raw(EntityID id, ComponentID cid, dib::structures::ErasedPtr value);
        
        /// Remove the component of provided type from the provided entity.
        template<IsComponent T>
        void remove_component(EntityID id)
        {
            remove_component(id, component_id<T>());
        }   
        /// Remove the component of provided type from the provided entity.
        void remove_component(EntityID id, ComponentID cid);

        /// Destroy the provided entity, immediately destructing all its components.
        void destroy_entity(EntityID id);

        friend class BasicQuery;
        friend struct EntityID;
        template<class... Comp> friend class Query;

        /// Get an entity query.
        template<IsComponent... Comp>
        Query<Comp...> query();
    };

    /// Get a reference to the component of the provided type from the provided entity.
    /// Throws if entity does not exist or does not have the specified component.
    template<IsComponent T>
    T &EntityID::get_component() const
    {
        return mem::read_as<T>(get_component_raw(component_id<T>()));
    }
    
    /// Check if an entity has the provided component types.
    template<IsComponent T>
    bool EntityID::has_component() const
    {
        return has_component(component_id<T>());
    }

    /// A class to store buffered entity commands to be applied later.
    class Commands
    {
        Entities *storage;
        dib::structures::ErasedStack entries;

        using CommandPtr = functional::fn<void(Commands *)>;

        template<class Lambda, class ...Args>
        void push_command(Args &&...args)
        {
            // Since we need to index our arguments without referring to the pack,
            // we nede to copy them into a tuple.
            dib::mem::Forgotten<std::tuple<std::remove_cvref_t<Args>...>> targs
            {
                FORWARD(args)...
            };

            entries.push(targs.value());
            entries.push((CommandPtr) [](Commands *self)
            {
                auto fn = Lambda{};

                auto &args = self->entries.top_as<std::tuple<std::remove_cvref_t<Args>...>>();
                fn(self->storage, static_cast<Args>(std::get<std::remove_cvref_t<Args>>(args))...);

                self->entries.pop();
            });

            /*
            // We first push all the arguments (as owned copies or moves) to the entry
            // stack. This guarantees they live for at least as long as this instance
            using ReversedArgs = meta::ListReverse::Call<meta::List<Args...>>;

            meta::each_list<ReversedArgs>([&]<class Arg>
            {
                using ArgNR = std::remove_cvref_t<Arg>;

                // LOGF("Pushing value of type {} to commands stack", refl::typeof<ArgNR>.name());
                entries.push<ArgNR>(std::get<dib::mem::Forgotten<ArgNR>>(targs).value());
            });

            // Finally, we push a generated lambda, which pops the arguments from the stack
            // and formats them correctly for passage to the provided lambda function.
            auto command = (CommandPtr) [](Commands *self)
            {
                auto fn = Lambda{};
                auto should_pop = false;

                std::apply(fn, std::tuple
                {
                    self->storage, 
                    [&]() -> Args &&
                    {
                        using Arg = std::remove_cvref_t<Args>;

                        if(should_pop)
                        {
                            self->entries.pop_nondestructive();
                        }

                        auto &arg = self->entries.top_as<Arg>();
                        should_pop = true;

                        return static_cast<Args &&>(arg);
                    }
                    ()...
                });
                
                if(should_pop)
                {
                    self->entries.pop_nondestructive();
                }

            };

            entries.push(command);
            */
        }

        template<IsComponent ...Components>
        using CreateEntity = decltype(
            [](Entities *en, Components &&...args) { en->create_entity<Components...>(FORWARD(args)...); });
            
        template<IsComponent Component>
        using AddComponent = decltype(
            [](Entities *en, EntityID id, Component &&args) { en->add_component<Component>(id, FORWARD(args)); });

        using DestroyEntity = decltype(
            [](Entities *en, EntityID id) { en->destroy_entity(id); });

        template<IsComponent Component>
        using RemoveComponent = decltype(
            [](Entities *en, EntityID id) { en->remove_component<Component>(id); });

    public:
        Commands(Entities *scene)
            : storage(scene), entries()
        {}

        Commands() = delete;
        Commands(const Commands &) = delete;
        Commands(Commands &&) = delete;

        /// Schedule the creation of an entity with the provided components.
        template<IsComponent ...T>
        Commands &create_entity(T &&...comp)
        {
            push_command<CreateEntity<T...>>(FORWARD(comp)...);
            return *this;
        }

        /// Schedule the destruction of an entity.
        Commands &destroy_entity(EntityID entity)
        {
            push_command<DestroyEntity>(entity);
            return *this;
        }
        
        /// Schedule the removal of a provided component type from an entity.
        template<IsComponent T>
        Commands &remove_component(EntityID entity)
        {
            push_command<RemoveComponent<T>>(entity);
            return *this;
        }
        
        /// Schedule the addition of a provided component type from an entity.
        template<IsComponent T>
        Commands &add_component(EntityID entity, T &&comp)
        {
            push_command<AddComponent<T>>(entity, FORWARD(comp));
            return *this;
        }

        /// Flush all pending commands.
        void flush();
    };

    // TODO: implement categorizations

    class BasicQuery
    {
    protected:
        Entities *entities;
        const Archetype *archetype;
        
        void for_each(dib::functional::FunctionRef<void(EntityID)>, bool sync) const;
        void for_each_exact(dib::functional::FunctionRef<void(EntityID)>, bool sync) const;

        BasicQuery(Entities *entities, const Archetype *archetype)
            : entities(entities)
            , archetype(archetype)
        {}

    public:
        void for_each(dib::functional::FunctionRef<void(EntityID)>) const;
        void for_each_sync(dib::functional::FunctionRef<void(EntityID)>) const;
        
        void for_each_exact(dib::functional::FunctionRef<void(EntityID)>) const;
        void for_each_exact_sync(dib::functional::FunctionRef<void(EntityID)>) const;
        
        size_t count_exact() const;
        size_t count() const;
    };

    /// The external representation of a query.
    template<class... Comp>
    class Query : public BasicQuery
    {
        using BasicQuery::BasicQuery;
        friend class Entities;

        auto convert_function(auto *lambda) const
        {
            return [=](EntityID id) -> void
            {
                (*lambda)(id, id.template get_component<Comp>()...);
            };
        }

    public:
        [[deprecated("Broken -- use sync variant. Will fix.")]]
        void for_each(std::invocable<EntityID, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each(f);
        }
        
        void for_each_sync(std::invocable<EntityID, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_sync(f);
        }
        
        [[deprecated("Broken -- use sync variant. Will fix.")]]
        void for_each_exact(std::invocable<EntityID, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_exact(f);
        }
        
        void for_each_exact_sync(std::invocable<EntityID, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_exact_sync(f);
        }
    };

    /// Get a query for the provided archetype.
    template<IsComponent... Comp>
    Query<Comp...> Entities::query()
    {
        auto &arch = archetype<Comp...>();

        register_query(arch);
        return Query<Comp...>{this, &arch};
    }

    /// Helper types to test if a class is a query type
    template<class> struct IsQueryType : std::false_type {};
    template<class... Comp> struct IsQueryType<Query<Comp...>> : std::true_type {};

    template<class Type>
    constexpr bool is_query = IsQueryType<Type>::value;

    template<class Type>
    concept IsQuery = is_query<Type>;
}