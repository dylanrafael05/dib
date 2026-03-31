#pragma once 

#include <algorithm>
#include <concepts>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
#include "dib/lambda.h"
#include "dib/vector.h"
#include "dib/reflect.h"

namespace dib::ecs
{
    struct Entities;
    struct Commands;
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

    /// An ID associated with each component type.
    struct ComponentID : public dib::types::Newtype<uint32_t>
    {
        refl::Type type() const;
    };

    /// An ID associated with each entity. Comprised of an index and version.
    struct Entity
    {
        uint64_t index : 40 = 0;
        uint64_t version : 24 = 0;

        Entity(uint64_t index, uint64_t version)
            : index(index), version(version)
        {}
        Entity()
            : index(INVALID_ID), version(0)
        {}

        bool is_invalid() const;

        constexpr bool operator==(const Entity &) const = default;

        template<class T> requires annotated_with<T, Component>
        bool has_component() const;

        template<class T> requires annotated_with<T, Component>
        T &get_component() const;

        template<class T> requires annotated_with<T, Component>
        void add_component(T &&value) const;
    };
    
    /// An ID associated with a categorization.
    class CategorizationID : public types::Newtype<size_t> 
    {};

    /// A helper type which behaves as a 'pack' of components; i.e. will be added
    /// as a group.
    template<class ...Components_>
    class ComponentPack
    {
        std::tuple<Components_...> _components;

    public:
        ComponentPack(std::add_rvalue_reference_t<Components_> ...components)
            : _components(MOVE(components)...)
        {}

        using Components = dib::meta::List<Components_...>;
        
        static_assert(
            meta::ListIsUnique::Eval<Components>, 
            "Components within a component pack must be unique by type.");

        template<size_t I>
        decltype(auto) get()
        {
            return MOVE(std::get<I>(_components));
        }

        template<class Component> 
            requires meta::ListContains::Eval<Components, Component>
        std::add_rvalue_reference_t<Component> get()
        {
            return MOVE(std::get<Component>(_components));
        }
    };

    using EnumerateComponentArgs = meta::Metafunction<
        meta::Type (meta::Type),
        meta::Match<
            METACASE(<class ...C>(ComponentPack<C...>) -> meta::Return<meta::List<C...>>),
            METACASE(<class C>(C) -> meta::Return<meta::List<C>>)
        >
    >;

    using CountComponentArgs = meta::Metafunction<
        size_t (meta::Type),
        METACASE(<class C>(C) -> meta::Return<meta::Auto<EnumerateComponentArgs::Call<C>::size>>)
    >;

    using FlattenComponentList = meta::Metafunction<
        meta::Type (meta::Type),
        METACASE(<class C>(C) -> meta::Return<
            meta::ListFoldr<meta::Type>::Call<
                meta::ListMap::Call<C, EnumerateComponentArgs>,
                meta::ListAppend,
                meta::List<>
            >>
        )
    >;

    template<class T>
    concept IsComponentPack = meta::Eval<METATEST(<class...C>(ComponentPack<C...>)), T>;
    
    template<class T>
    concept NotComponentPack = !IsComponentPack<T>;

    constexpr decltype(auto) splat_component_args(auto &&fn, auto &&...values)
    {
        return meta::splat_args<EnumerateComponentArgs>(
            FORWARD(fn),
            functional::Overload
            {
                []<size_t I>(IsComponentPack auto &&pack) { return pack.template get<I>(); },
                []<size_t I>(auto &&other) { return other; }
            },
            FORWARD(values)...
        );
    };

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
    template<NotComponentPack T> requires annotated_with<T, Component>
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
    template<class... Comp>
    const Archetype &archetype() requires (!IsComponentPack<Comp> && ...)
    {
        static Archetype result = Archetype::of_components<Comp...>();
        return result;
    }

    template<class... Comp> requires (IsComponentPack<Comp> || ...)
    const Archetype &archetype()
    {
        using Components = FlattenComponentList::Call<meta::List<Comp...>>;

        return meta::splat_list<Components>(
            []<class... L> -> decltype(auto) { return archetype<L...>(); });
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
            std::vector<Entity> entity_ids;
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
        void assert_exists(Entity id) const;
        
        /// Assert that the provided entity has the provided component.
        void assert_has_component(Entity id, ComponentID cid) const;

        // TODO: implement groupings!

    public:
        Entities();

        Entities(const Entities&) = delete;
        Entities(Entities &&) = delete;

        threading::ThreadScheduler &thread_scheduler() { return scheduler; }

        /// Check if the provided entity is alive and well.
        bool is_alive(Entity id) const;

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
        Entity create_entity();
        
        /// Create a new, uninitialized entity of the provided archetype.
        Entity create_uninitialized_entity(const Archetype &archetype);

        template<types::IsValue... Components> requires (annotated_with<Components, Component> && ...)
        Entity create_entity(Components &&...components)
        {
            if constexpr((IsComponentPack<Components> || ...))
            {
                return splat_component_args(
                    DIB_OVERSET(create_entity), 
                    FORWARD(components)...);
            }
            else
            {
                auto &archetype = dib::ecs::archetype<Components...>();
                auto entity = create_uninitialized_entity(archetype);

                ([&]
                {
                    using Component = Components;
                    dib::mem::Forgotten<Component> cons_comp(MOVE(components));

                    auto &component = get_component<Component>(entity);
                    dib::uninitialized_relocate((Component *) &cons_comp, &component);

                }(), ...); 

                return entity;
            }
        }

        void *get_component_raw(Entity id, ComponentID cid);
        const void *get_component_raw(Entity id, ComponentID cid) const;

        /// Get a reference to the component of the provided type from the provided entity.
        /// Throws if entity does not exist or does not have the specified component.
        /// Does not handle component packs.
        template<NotComponentPack T> requires types::IsValue<T>
        T &get_component(Entity id)
        {
            return mem::read_as<T>(get_component_raw(id, component_id<T>()));
        }

        /// const variant of get_component
        template<NotComponentPack T> requires types::IsValue<T>
        const T &get_component(Entity id) const
        {
            return const_cast<Entities*>(this)->get_component<T>(id);
        }

        /// Get the archetype of an entity
        inline const Archetype &get_archetype(Entity id) const
        {
            assert_exists(id);
            return archetype_stores[entity_map[id.index].archetype].archetype;
        }
        
        /// Check if the given entity has the provided component type.
        bool has_component(Entity id, ComponentID cid) const;

        /// Check if an entity has the provided component types.
        /// Properly handles component packs.
        template<NotComponentPack ...T> requires (types::IsValue<T> && ...)
        bool has_component(Entity id) const
        {
            if constexpr ((IsComponentPack<T> || ...))
            {
                using Components = FlattenComponentList::Call<meta::List<T...>>;

                return meta::splat_list<Components>([&]<class ...C>
                {
                    has_component<C...>(id);
                });
            }
            else
            {
                return (has_component(id, component_id<T>()) || ...);
            }
        }

        /// Add new components to the provided entity.
        /// Properly handles component packs.
        template<types::IsValue ...T>
        void add_component(Entity id, T &&...component)
        {
            if constexpr ((IsComponentPack<T> || ...))
            {
                splat_component_args(
                    DIB_LMB_n(add_component(id, _args...)), 
                    FORWARD(component)...);
            }
            else
            {
                ([&]{
                    dib::mem::Forgotten<T> value(MOVE(component));

                    add_component_raw(id, component_id<T>(), (void*) &value);
                }(), ...);
            }
        }

        /// Remove the components of provided type from the provided entity.
        /// Properly handles component packs.
        template<types::IsValue ...T>
        void remove_component(Entity id)
        {
            if constexpr((IsComponentPack<T> || ...))
            {
                using Components = FlattenComponentList::Call<meta::List<T...>>;

                meta::splat_list<Components>([&]<class ...C>
                {
                    remove_component<C...>(id);
                });
            }
            else
            {
                ([&]{
                    remove_component(id, component_id<T>());
                }(), ...);
            }
        }

        /// Add a new component to the provided entity. The component structure must be
        /// located at 'value' and must have type of 'cid'.
        void add_component_raw(Entity id, ComponentID cid, dib::structures::ErasedPtr value);
        /// Remove the component of provided type from the provided entity.
        void remove_component(Entity id, ComponentID cid);
        /// Destroy the provided entity, immediately destructing all its components.
        void destroy_entity(Entity id);

        friend class BasicQuery;
        template<class... Comp> friend class Query;

        /// Get an entity query.
        template<NotComponentPack... Comp>
        Query<Comp...> query();
    };

    template<class T> requires annotated_with<T, Component>
    bool Entity::has_component() const
    {
        return 
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
            std::tuple<dib::mem::Forgotten<std::remove_cvref_t<Args>>...> targs
            {
                FORWARD(args)...
            };

            // We first push all the arguments (as owned copies or moves) to the entry
            // stack. This guarantees they live for at least as long as this instance
            using ReversedArgs = meta::ListReverse::Call<meta::List<Args...>>;

            meta::each_list<ReversedArgs>([&]<class Arg>
            {
                using ArgNR = std::remove_cvref_t<Arg>;

                LOGF("Pushing value of type {} to commands stack", refl::typeof<ArgNR>.name());
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
        }

        template<class ...Components>
        using CreateEntity = decltype(
            [](Entities *en, Components &&...args) { en->create_entity<Components...>(FORWARD(args)...); });
            
        template<class ...Components>
        using AddComponent = decltype(
            [](Entities *en, Entity id, Components &&...args) { en->add_component<Components...>(id, FORWARD(args)...); });

        using DestroyEntity = decltype(
            [](Entities *en, Entity id) { en->destroy_entity(id); });

        template<class ...Components>
        using RemoveComponent = decltype(
            [](Entities *en, Entity id) { en->remove_component<Components...>(id); });

    public:
        Commands(Entities *scene)
            : storage(scene), entries()
        {}

        Commands() = delete;
        Commands(const Commands &) = delete;
        Commands(Commands &&) = delete;

        /// Schedule the creation of an entity with the provided components.
        template<class ...T>
        Commands &create_entity(T &&...comp)
        {
            push_command<CreateEntity<T...>>(FORWARD(comp)...);
            return *this;
        }

        /// Schedule the destruction of an entity.
        Commands &destroy_entity(Entity entity)
        {
            push_command<DestroyEntity>(entity);
            return *this;
        }
        
        /// Schedule the removal of a provided component type from an entity.
        template<class ...T>
        Commands &remove_component(Entity entity)
        {
            push_command<RemoveComponent<T...>>(entity);
            return *this;
        }
        
        /// Schedule the addition of a provided component type from an entity.
        template<class ...T>
        Commands &add_component(Entity entity, T &&...comp)
        {
            push_command<AddComponent<T...>>(entity, FORWARD(comp)...);
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
        
        void for_each(dib::functional::FunctionRef<void(Entity)>, bool sync) const;
        void for_each_exact(dib::functional::FunctionRef<void(Entity)>, bool sync) const;

        BasicQuery(Entities *entities, const Archetype *archetype)
            : entities(entities)
            , archetype(archetype)
        {}

    public:
        void for_each(dib::functional::FunctionRef<void(Entity)>) const;
        void for_each_sync(dib::functional::FunctionRef<void(Entity)>) const;
        
        void for_each_exact(dib::functional::FunctionRef<void(Entity)>) const;
        void for_each_exact_sync(dib::functional::FunctionRef<void(Entity)>) const;
        
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
            return [&](Entity id) -> void
            {
                (*lambda)(id, this->entities->get_component<Comp>(id)...);
            };
        }

    public:
        [[deprecated("Broken -- use sync variant. Will fix.")]]
        void for_each(std::invocable<Entity, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each(f);
        }
        
        void for_each_sync(std::invocable<Entity, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_sync(f);
        }
        
        [[deprecated("Broken -- use sync variant. Will fix.")]]
        void for_each_exact(std::invocable<Entity, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_exact(f);
        }
        
        void for_each_exact_sync(std::invocable<Entity, Comp &...> auto &&lambda) const
        {
            auto f = convert_function(&lambda);
            BasicQuery::for_each_exact_sync(f);
        }
    };

    /// Get a query for the provided archetype.
    template<NotComponentPack... Comp>
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