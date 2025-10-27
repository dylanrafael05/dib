#ifndef __ECS_COMPONENTS_H
#define __ECS_COMPONENTS_H

#include <typeindex>
#include <typeinfo>

#include <unordered_map>
#include <vector>
#include <tuple>
#include <stdint.h>

#include "../sparse_bitset.h"
#include "../raw_memory.h"
#include "../bijection.h"

/// <summary>
///  ecs functions; all components used within an ecs scene must be trivially relocatable
/// </summary>
namespace dib::ecs
{
    constexpr static uint64_t INVALID_ID = 0xFFFFFFFFFF;

    using ComponentID = uint32_t;
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

    // Basics //
    //const char *component_name(ComponentID id);
    //types::TypeDescriptor component_desc(ComponentID id);

    namespace detail
    {
        //ComponentID component_id_with_desc(const std::type_index &type, types::TypeDescriptor desc);
    }
    
    //template<class T>
    //ComponentID component_type()
    //{
    //    static const ComponentID cache = detail::component_id_with_desc(typeid(T), types::TypeDescriptor::for_type<T>());
    //    return cache;
    //}

    namespace detail
    {
        inline auto &id_map()
        {
            static dib::structures::Bijection<types::TypeDescriptor, ComponentID> value;
            return value;
        }

        inline auto &max_id()
        {
            static ComponentID value;
            return value;
        }

        inline ComponentID dense_component_type(types::TypeDescriptor desc)
        {
            auto it = detail::id_map().find(desc);
            if (it != detail::id_map().end()) return it->second;

            auto new_id = detail::max_id()++;
            detail::id_map().insert(desc, new_id);

            return new_id;
        }

        template<class T>
        inline ComponentID dense_component_type()
        {
            return dense_component_type(types::typedesc<T>);
        }
    }

    struct ArchetypeStorage;

    struct ArchetypeEdge
    {
        size_t on_add = static_cast<size_t>(-1);
        size_t on_remove = static_cast<size_t>(-1);
    };

    using Archetype = dib::structures::SparseBitset;

    template<class... Comp>
    Archetype make_archetype()
    {
        return Archetype::from_bits({detail::dense_component_type<Comp>()...});
    }
    
    struct ArchetypeStorage
    {
        Archetype archetype;
        size_t index;

        std::unordered_map<types::TypeDescriptor, dib::structures::ErasedVec> storage;
        std::vector<EntityID> entity_ids;
        std::vector<size_t> free_indices;
        size_t capacity;

        std::unordered_map<types::TypeDescriptor, ArchetypeEdge> related_archetypes;

        ArchetypeStorage() 
            : capacity(0), index(0)
        {}

        ArchetypeStorage(ArchetypeStorage &&other) noexcept
            : archetype(std::move(other.archetype)), index(other.index), 
              storage(std::move(other.storage)), entity_ids(std::move(other.entity_ids)),
              free_indices(std::move(other.free_indices)), capacity(other.capacity),
              related_archetypes(std::move(other.related_archetypes))
        {
            other.capacity = 0;
        }

        ~ArchetypeStorage();
    };

    struct EntityEntry
    {
        size_t archetype;
        size_t index;

        uint64_t version;
    };

    class BasicQueryIterator;
    template<class ...Components> class QueryIterator;
    template<class ...Components> class Query;

    class Commands;

    class Entities
    {
        std::vector<EntityEntry> entity_map;
        std::vector<uint64_t> free_ids;

        std::vector<ArchetypeStorage> archetypes;
        std::unordered_map<Archetype, size_t> archetype_map;
        std::unordered_map<Archetype, std::vector<size_t>> query_map;

        size_t register_archetype(ArchetypeStorage &&in);
        void register_query(const Archetype &in);

        ArchetypeStorage &modify_archetype(types::TypeDescriptor cid, size_t archetype_idx, bool add);

        uint64_t alloc_entity();
        uint64_t alloc_in_arch(ArchetypeStorage &storage);
        void dealloc_entity(uint64_t index);

        bool has_component(EntityID id, types::TypeDescriptor cid) const;

        inline void assert_exists(EntityID id) const
        {
        #ifndef NDEBUG
            if(entity_map.size() < id.index || entity_map[id.index].version != id.version)
            {
                std::cerr << "Invalid access to entity with id " << id.index << ".\n";
                std::abort();
            }
        #endif
        }
        
        inline void assert_has_component(EntityID id, types::TypeDescriptor cid) const
        {
        #ifndef NDEBUG
            if(!has_component(id, cid))
            {
                std::cerr << "Attempt to access a component which does not exist.\n";
                std::abort();
            }
        #endif
        }

    public:
        Entities();

        Entities(const Entities&) = delete;
        Entities(Entities &&) = delete;

        EntityID create_entity();

        template<class T>
        T &get_component(EntityID id)
        {
            if constexpr(dib::types::packed_sizeof<T> == 0)
            {
                return *(T*)nullptr;
            }

            auto cid = types::typedesc<T>;

            assert_exists(id);
            assert_has_component(id, cid);
            
            auto &entry = entity_map[id.index];
            auto &storage = archetypes[entry.archetype];
            auto &vec = storage.storage[cid];

            return vec.template get<T>(entry.index);
        }

        template<class T>
        const T &get_component(EntityID id) const
        {
            return const_cast<Entities*>(this)->get_component<T>(id);
        }

        inline const Archetype &get_archetype(EntityID id) const
        {
            assert_exists(id);
            return archetypes[entity_map[id.index].archetype].archetype;
        }

        template<class T>
        bool has_component(EntityID id) const
        {
            assert_exists(id);
            return has_component(id, types::typedesc<T>);
        }

        template<class T, class... Args>
        void add_component(EntityID id, Args &&...args)
        {
            auto cid = component_type<T>();
            dib::mem::Forgotten<T> value(std::forward<Args>(args)...);

            add_component(id, cid, (void*) &value);
        }

        template<class T>
        void remove_component(EntityID id)
        {
            auto cid = component_type<T>(id);
            remove_component(id, cid);
        }

        void add_component(EntityID id, types::TypeDescriptor cid, dib::structures::ErasedPtr value);
        void remove_component(EntityID id, types::TypeDescriptor cid);
        void destroy_entity(EntityID id);

        friend class BasicQueryIterator;
        template<class... Comp> friend class QueryIterator;
        template<class... Comp> friend class Query;

        template<class... Comp>
        Query<Comp...> query();
    };

    class Commands
    {
        Entities *storage;
        dib::structures::InhomogeneousStack entries;

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

        template<class... Comp>
        Commands &create_entity(Comp &&...comp)
        {
            ([&]{
                
                entries.push(std::forward<Comp>(comp));

            }(), ...);

            push_create_entity(sizeof...(Comp));
            return *this;
        }

        Commands &destroy_entity(EntityID entity)
        {
            push_destroy_entity(entity);
            return *this;
        }
        
        template<class T>
        Commands &remove_component(EntityID entity)
        {
            push_remove_component(entity, types::typedesc<T>);
            return *this;
        }
        
        template<class T>
        Commands &add_component(EntityID entity, T &&component)
        {
            using CType = std::remove_cvref_t<T>;

            entries.push(std::forward<T>(component));
            push_add_component(entity);

            return *this;
        }

        void flush();
    };

    class BasicQueryIterator
    {
        Entities *world;
        const std::vector<size_t> *arch;
        size_t arch_it;
        size_t place_it;

        struct End_t {};
        constexpr static End_t end = {};

        BasicQueryIterator(Entities *world, const std::vector<size_t> &arch);
        BasicQueryIterator(End_t, Entities *world, const std::vector<size_t> &arch);

        inline ArchetypeStorage &storage() const {return world->archetypes[(*arch)[arch_it]];}

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
            return world == other.world
                && arch == other.arch
                && arch_it == other.arch_it
                && place_it == other.place_it;
        }

        bool operator!=(const BasicQueryIterator &other) const
        {
            return !operator==(other);
        }
    };

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

    template<class... Comp>
    class QueryIterator
    {
        BasicQueryIterator iter;

        QueryIterator(BasicQueryIterator &&query)
            : iter(std::move(query))
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
                std::get<Comp*>(output) = &iter.storage().storage[types::typedesc<Component>].template get<Component>(iter.place_it);

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

    template<class... Comp>
    class Query
    {
        Entities *scene;
        const Archetype *arch;

        Query(Entities *scene, const Archetype *arch)
            : scene(scene), arch(arch)
        {}

        friend class Entities;

    public:
        QueryIterator<Comp...> begin()
        {
            return {
                BasicQueryIterator{
                    scene, scene->query_map.at(*arch)
                }
            };
        }

        QueryIterator<Comp...> end()
        {
            return {
                BasicQueryIterator{
                    BasicQueryIterator::end, scene, scene->query_map.at(*arch)
                }
            };
        }

        size_t count() const
        {
            size_t out = 0;
            for (auto &set : scene->query_map.at(*arch))
            {
                out += scene->archetypes[set].capacity - scene->archetypes[set].free_indices.size();
            }
            return out;
        }
    };

    template<class... Comp>
    Query<Comp...> Entities::query()
    {
        static const Archetype arch = make_archetype<Comp...>();

        register_query(arch);
        return Query<Comp...>{this, &arch};
    }

    template<class> struct IsQueryType : std::false_type {};
    template<class... Comp> struct IsQueryType<Query<Comp...>> : std::true_type {};

    template<class Type>
    constexpr bool is_query = IsQueryType<Type>::value;
}

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

#endif