#pragma once 

#include <unordered_map>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <format>

#include "dib/sparse_bitset.h"
#include "dib/raw_memory.h"
#include "dib/bijection.h"

/// ecs functions
namespace dib::ecs
{
    constexpr static uint64_t INVALID_ID = 0xFFFFFFFFFF;

    /// An ID associated with each component type.
    using ComponentID = uint32_t;

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

        bool is_invalid() const;
    };

    /// Implementation details for component id mapping below
    namespace detail
    {
        /// Map from component type descriptor to component ID and vice-versa
        inline auto &component_id_map()
        {
            static dib::structures::Bijection<types::TypeDescriptor, ComponentID> value;
            return value;
        }

        /// Maximum component ID so far
        inline auto &next_free_component_id()
        {
            static ComponentID value;
            return value;
        }

        /// Get the component id of a provided type descriptor
        inline ComponentID dense_component_type(types::TypeDescriptor desc)
        {
            auto it = detail::component_id_map().find(desc);
            if (it != detail::component_id_map().end()) return it->second;

            auto new_id = detail::next_free_component_id()++;
            detail::component_id_map().insert(desc, new_id);

            return new_id;
        }

        /// Get the component id of a provided component type
        template<class T>
        inline ComponentID dense_component_type()
        {
            return dense_component_type(types::typedesc<T>);
        }
    }

    /// In this ECS architecture, entities are grouped by 'archetype'.
    /// An entity's 'archetype' is the set of components which comprise it.
    /// The components for each entity are stored in densely packed ErasedVec`s
    /// grouped by entity archetype.

    /// The storage for all entity data within one archetype.
    struct ArchetypeStorage;

    /// An ID used to identify archetypes and their corresponding entity stores.
    using ArchetypeID = size_t;

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

    /// An entity's archetype
    using Archetype = dib::structures::SparseBitset;

    /// Get the representation of the provided archetype in-memory.
    /// There is exactly one archetype allocated for each *ordered* set of types
    /// provided to this function.
    template<class... Comp>
    Archetype &archetype()
    {
        static Archetype result = Archetype::from_bits({detail::dense_component_type<Comp>()...});
        return result;
    }
    
    struct ArchetypeStorage
    {
        /// The archetype (and associated archetype storage index) of this storage unit
        Archetype archetype;
        ArchetypeID index;

        /// The map of all component stores by their associated component type
        std::unordered_map<types::TypeDescriptor, dib::structures::ErasedVec> component_stores;

        /// A list of the entities stored within this archetype storage, as well as which
        /// indices currently refer to a 'free' entity
        std::vector<EntityID> entity_ids;
        std::vector<size_t> free_indices;
        size_t capacity;

        std::unordered_map<types::TypeDescriptor, ArchetypeEdge> related_archetypes;

        ArchetypeStorage() 
            : index(0), capacity(0)
        {}

        ArchetypeStorage(ArchetypeStorage &&other) noexcept
            : archetype(MOVE(other.archetype)), index(other.index), 
              component_stores(MOVE(other.component_stores)), entity_ids(MOVE(other.entity_ids)),
              free_indices(MOVE(other.free_indices)), capacity(other.capacity),
              related_archetypes(MOVE(other.related_archetypes))
        {
            other.capacity = 0;
        }

        ~ArchetypeStorage();
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

    /// Forward declare all the content related to queries.
    class BasicQueryIterator;
    template<class ...Components> class QueryIterator;
    template<class ...Components> class Query;
    class Commands;

    /// The 'meat' of the ECS system; a class responsible for all
    /// entity and component interactions.
    class Entities
    {
        /// The list of entity entries, indexed by entity index.
        std::vector<EntityEntry> entity_map;
        std::vector<uint64_t> free_ids;

        /// The list of archetype stores, indexed by archetype id.
        /// Mappings from archetype to id and from query archetype to
        /// vector of id are provided.
        ///
        /// It is important to note that the first archetype (id 0)
        /// will always be the 'empty' archetype.
        std::vector<ArchetypeStorage> archetype_stores;
        std::unordered_map<Archetype, ArchetypeID> archetype_map;
        std::unordered_map<Archetype, std::vector<ArchetypeID>> query_map;

        /// Register a *new* archetype storage with the entities system.
        /// Returns the ID now associated with that archetype.
        ArchetypeID register_archetype(ArchetypeStorage &&in);

        /// Register a *new* query archetype with the entities system.
        void register_query(const Archetype &in);

        /// Modify the provided archetype (by id) by either adding or removing the
        /// provided component type, updating all internal storage relating to archetypes
        /// and caching if possible.
        ArchetypeStorage &add_or_remove_from_archetype(types::TypeDescriptor cid, ArchetypeID archetype, bool add);

        /// Allocate space for a new, empty entity.
        uint64_t alloc_entity();
        /// Allocate space within the provided archetype store for a new entity.
        uint64_t alloc_in_arch(ArchetypeStorage &storage);
        /// Free the provided entity.
        void dealloc_entity(uint64_t index);

        /// Check if the given entity has the provided component type.
        bool has_component(EntityID id, types::TypeDescriptor cid) const;

        /// Assert that the provided entity exists.
        inline void assert_exists(EntityID id) const
        {
            if(entity_map.size() < id.index || entity_map[id.index].version != id.version)
            {
                RUNTIME_ERROR(std::format("Invalid access to entity with id {}.", auto(id.index)));
            }
        }
        
        /// Assert that the provided entity has the provided component.
        inline void assert_has_component(EntityID id, types::TypeDescriptor cid) const
        {
            if(!has_component(id, cid))
            {
                RUNTIME_ERROR(std::format("Attempt to access a component of type {} which does not exist.", cid.name()));
            }
        }

    public:
        Entities();

        Entities(const Entities&) = delete;
        Entities(Entities &&) = delete;

        /// Create a new, empty entity.
        EntityID create_entity();

        /// Get a reference to the component of the provided type from the provided entity.
        /// Throws if entity does not exist or does not have the specified component.
        template<class T>
        T &get_component(EntityID id)
        {
            auto cid = types::typedesc<T>;

            assert_exists(id);
            assert_has_component(id, cid);
            
            if constexpr(dib::types::packed_sizeof<T> == 0)
            {
                return *mem::pointer_to_zst<T>();
            }
            
            auto &entry = entity_map[id.index];
            auto &storage = archetype_stores[entry.archetype];
            auto &vec = storage.component_stores[cid];

            return vec.template get<T>(entry.index);
        }

        /// const variant of get_component
        template<class T>
        const T &get_component(EntityID id) const
        {
            return const_cast<Entities*>(this)->get_component<T>(id);
        }

        /// Get the archetype of an entity
        inline const Archetype &get_archetype(EntityID id) const
        {
            assert_exists(id);
            return archetype_stores[entity_map[id.index].archetype].archetype;
        }

        /// Check if an entity has the provided component type
        template<class T>
        bool has_component(EntityID id) const
        {
            assert_exists(id);
            return has_component(id, types::typedesc<T>);
        }

        /// Add a new component to the provided entity
        template<class T, class... Args>
        void add_component(EntityID id, Args &&...args)
        {
            auto cid = types::typedesc<T>;
            dib::mem::Forgotten<T> value(FORWARD(args)...);

            add_component_raw(id, cid, (void*) &value);
        }

        /// Remove the component of provided type from the provided entity
        template<class T>
        void remove_component(EntityID id)
        {
            auto cid = types::typedesc<T>;
            remove_component(id, cid);
        }

        /// Add a new component to the provided entity. The component structure must be
        /// located at 'value' and must have type of 'cid'.
        void add_component_raw(EntityID id, types::TypeDescriptor cid, dib::structures::ErasedPtr value);
        /// Remove the component of provided type from the provided entity.
        void remove_component(EntityID id, types::TypeDescriptor cid);
        /// Destroy the provided entity, immediately destructing all its components.
        void destroy_entity(EntityID id);

        friend class BasicQueryIterator;
        template<class... Comp> friend class QueryIterator;
        template<class... Comp> friend class Query;

        /// Get an entity query.
        template<class... Comp>
        Query<Comp...> query();
        /// Get an entity query which only matches entities with exactly the provided
        /// set of components and no more.
        template<class... Comp>
        Query<Comp...> query_exact();
    };

    /// A class to store buffered entity commands to be applied later.
    class Commands
    {
        Entities *storage;
        dib::structures::ErasedStack entries;

        void push_create_entity(uint8_t component_count);
        void push_destroy_entity(EntityID entity);
        void push_remove_component(EntityID entity, types::TypeDescriptor id);
        void push_add_component(EntityID entity);

    public:
        Commands(Entities *scene)
            : storage(scene), entries()
        {}

        Commands() = delete;
        Commands(const Commands &) = delete;
        Commands(Commands &&) = delete;

        /// Schedule the creation of an entity with the provided components.
        template<class... Comp>
        Commands &create_entity(Comp &&...comp)
        {
            ([&]{
                
                entries.push(FORWARD(comp));

            }(), ...);

            push_create_entity(sizeof...(Comp));
            return *this;
        }

        /// Schedule the destruction of an entity.
        Commands &destroy_entity(EntityID entity)
        {
            push_destroy_entity(entity);
            return *this;
        }
        
        /// Schedule the removal of a provided component type from an entity.
        template<class T>
        Commands &remove_component(EntityID entity)
        {
            push_remove_component(entity, types::typedesc<T>);
            return *this;
        }
        
        /// Schedule the addition of a provided component type from an entity.
        template<class T>
        Commands &add_component(EntityID entity, T &&component)
        {
            entries.push(FORWARD(component));
            push_add_component(entity);

            return *this;
        }

        /// Flush all pending commands.
        void flush();
    };

    /// The basic, non-generic implementation of query iteration.
    /// Can handle both exact and non-exact queries of all archetypes.
    class BasicQueryIterator
    {
        Entities *entities;
        const std::vector<ArchetypeID> *arch;
        ArchetypeID singlet_arch;
        size_t arch_it;
        size_t place_it;

        struct End_t {};
        constexpr static End_t end = {};

        BasicQueryIterator(Entities *world, const std::vector<ArchetypeID> &arch);
        BasicQueryIterator(End_t, Entities *world, const std::vector<ArchetypeID> &arch);

        BasicQueryIterator(Entities *world, ArchetypeID singlet);
        BasicQueryIterator(End_t, Entities *world, ArchetypeID singlet);

        inline ArchetypeStorage &storage() const 
        {
            if(!arch) return entities->archetype_stores[singlet_arch];
            return entities->archetype_stores[(*arch)[arch_it]];
        }

        void advance_to_valid();
        void advance();

        template<class...> friend class QueryIterator;
        template<class...> friend class Query;

    public:
        EntityID operator*() const
        {
            return storage().entity_ids[place_it];
        }

        BasicQueryIterator &operator++()
        {
            advance();
            return *this;
        }

        BasicQueryIterator operator++(int)
        {
            auto cpy = *this;
            advance();
            return cpy;
        }

        bool operator==(const BasicQueryIterator &other) const
        {
            return entities == other.entities
                && arch == other.arch
                && arch_it == other.arch_it
                && place_it == other.place_it;
        }

        bool operator!=(const BasicQueryIterator &other) const
        {
            return !operator==(other);
        }
    };

    /// A representation of a single element in a query; 
    /// stores the entity ID as well as a tuple of references to its
    /// components.
    template<class... Comp>
    struct QueryResult
    {
        EntityID id;
        std::tuple<Comp &...> components;
        
        template<size_t N>
        decltype(auto) get()
        {
            if constexpr(N != 0)
            {
                return std::get<N - 1>(components);
            }
            else 
            {
                return (id);
            }
        }
        
        template<class T>
        decltype(auto) get()
        {
            return std::get<T&>(components);
        }
    };

    /// The generic version of a query iterator which supports
    /// getting the query result.
    template<class... Comp>
    class QueryIterator
    {
        BasicQueryIterator iter;

        QueryIterator(BasicQueryIterator &&query)
            : iter(MOVE(query))
        {}

        friend class Entities;
        template<class...> friend class Query;

    public:
        QueryResult<Comp...> operator*()
        {
            auto id = *iter;

            std::tuple<Comp *...> output = { (Comp*)nullptr... };

            ([&]{

                using Component = std::remove_const_t<Comp>;
                std::get<Comp*>(output) = &iter.storage().component_stores[types::typedesc<Component>].template get<Component>(iter.place_it);

            }(), ...);

            std::tuple<Comp &...> ref_output = { *std::get<Comp*>(output)... };

            return QueryResult<Comp...>
            {
                .id = id,
                .components = ref_output
            };
        }

        QueryIterator &operator++()
        {
            iter++;
            return *this;
        }

        QueryIterator operator++(int)
        {
            auto cpy = *this;
            iter++;
            return cpy;
        }

        bool operator==(const QueryIterator &other) const
        {
            return iter == other.iter;
        }
        bool operator!=(const QueryIterator &other) const
        {
            return !operator==(other);
        }
    };

    /// An iterable class which denotes a query for entities of a provided
    /// archetype (either all that match or all that are exactly that archetype).
    template<class... Comp>
    class Query
    {
        Entities *entities;
        const Archetype *arch;
        bool _is_exact;

        Query(Entities *scene, const Archetype *arch, bool is_exact = false)
            : entities(scene), arch(arch), _is_exact(is_exact)
        {}

        friend class Entities;

    public:
        QueryIterator<Comp...> begin()
        {
            if(_is_exact)
            {
                return {
                    BasicQueryIterator{
                        entities, entities->archetype_map.at(*arch)
                    }
                };
            }

            return {
                BasicQueryIterator{
                    entities, entities->query_map.at(*arch)
                }
            };
        }

        QueryIterator<Comp...> end()
        {
            if(_is_exact)
            {
                return {
                    BasicQueryIterator{
                        BasicQueryIterator::end, entities, entities->archetype_map.at(*arch)
                    }
                };
            }

            return {
                BasicQueryIterator{
                    BasicQueryIterator::end, entities, entities->query_map.at(*arch)
                }
            };
        }

        size_t count() const
        {
            if(_is_exact)
            {
                return entities->archetype_stores[entities->archetype_map[*arch]].capacity 
                     - entities->archetype_stores[entities->archetype_map[*arch]].free_indices.size();
            }

            size_t out = 0;
            for (auto &set : entities->query_map.at(*arch))
            {
                out += entities->archetype_stores[set].capacity 
                     - entities->archetype_stores[set].free_indices.size();
            }
            return out;
        }
    };

    /// Get a query for the provided archetype.
    template<class... Comp>
    Query<Comp...> Entities::query()
    {
        auto &arch = archetype<Comp...>();

        register_query(arch);
        return Query<Comp...>{this, &arch};
    }
    
    /// Get a query for exactly the provided archetype.
    template<class... Comp>
    Query<Comp...> Entities::query_exact()
    {
        auto &arch = archetype<Comp...>();

        register_query(arch);
        return Query<Comp...>{this, &arch, true};
    }

    /// Helper types to test if a class is a query type
    template<class> struct IsQueryType : std::false_type {};
    template<class... Comp> struct IsQueryType<Query<Comp...>> : std::true_type {};

    template<class Type>
    constexpr bool is_query = IsQueryType<Type>::value;

    template<class Type>
    concept IsQuery = is_query<Type>;
}

/// Allow QueryResult to be used in a structured binding.
namespace std
{
    template<class... Comp>
    struct tuple_size<dib::ecs::QueryResult<Comp...>> : integral_constant<size_t, 1 + sizeof...(Comp)>
    {};

    template<class... Comp>
    struct tuple_element<0, dib::ecs::QueryResult<Comp...>>
    {
        using type = dib::ecs::EntityID;
    };
    
    template<class... Comp, size_t N>
    struct tuple_element<N, dib::ecs::QueryResult<Comp...>>
    {
        using type = decltype(std::get<N - 1>(std::declval<dib::ecs::QueryResult<Comp...>>().components));
    };
}