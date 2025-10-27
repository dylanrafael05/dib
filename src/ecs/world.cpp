#include "dib/ecs/components.h"
#include "dib/ecs/world.h"

#include "dib/debug.h"

#include <unordered_map>
#include <iostream>

using namespace dib::ecs;
namespace ecs = dib::ecs;
namespace types = dib::types;

/*
struct ComponentEntry
{
    const char *name;
    dib::types::TypeDescriptorPtr desc;
};

static std::unordered_map<std::type_index, ComponentID> type_to_id;
static std::unordered_map<ComponentID, ComponentEntry> id_to_entry;
*/

bool EntityID::is_invalid() const
{
    return index == INVALID_ID;
}

/*
const char *ecs::component_name(ComponentID id)
{
    auto it = id_to_entry.find(id);

    if (it == id_to_entry.end())
    {
        std::cerr << "Cannot get the component name of non-registered id " << id << "." << std::endl;
        std::abort();
    }

    return it->second.name;
}

types::TypeDescriptorPtr ecs::component_desc(ComponentID id)
{
    auto it = id_to_entry.find(id);

    if (it == id_to_entry.end())
    {
        std::cerr << "Cannot get the component descriptor of non-registered id " << id << "." << std::endl;
        std::abort();
    }

    return it->second.desc;
}

ComponentID ecs::detail::component_id_with_desc(const std::type_index &type, types::TypeDescriptorPtr desc)
{
    auto it = type_to_id.find(type);

    if (it == type_to_id.end())
    {
        it = type_to_id.insert({ type, (ComponentID)type_to_id.size() }).first;
        id_to_entry.insert_or_assign(it->second, ComponentEntry{ .name = type.name(), .desc = desc});
    }

    return it->second;
}
*/

// Archetypes //
ArchetypeStorage::~ArchetypeStorage()
{
    for(size_t i = 0; i < capacity; i++)
    {
        for(auto &elem : free_indices)
        {
            if(elem == i) goto end_of_loop;
        }
        
        for(auto &[_, vec] : storage)
        {
            vec.inplace_destruct(i);
        }

        end_of_loop:;
    }

    for(auto &[_, vec] : storage)
    {
        vec.deallocate();
    }
}

// World //
Entities::Entities()
{
    register_archetype({});
}

size_t Entities::register_archetype(ArchetypeStorage &&in)
{
    auto index = archetypes.size();
    archetypes.push_back(std::move(in));

    auto &archetype = archetypes.back();
    archetype.index = index;

    archetype_map[archetype.archetype] = index;
    for(auto &[query, indexes] : query_map)
    {
        if(query.is_subset_of(archetype.archetype))
        {
            indexes.push_back(index);
        }
    }

    return index;
}

void Entities::register_query(const Archetype &in)
{
    if(query_map.count(in) > 0)
        return;
    
    std::vector<size_t> matching;

    for(size_t i = 0; i < archetypes.size(); i++)
    {
        if(in.is_subset_of(archetypes[i].archetype))
        {
            matching.push_back(i);
        }
    }

    query_map[in] = std::move(matching);
}

ArchetypeStorage &Entities::modify_archetype(types::TypeDescriptor cid, size_t archetype_idx, bool add) 
{
    auto related_get = [&](const ArchetypeEdge &edge)
    {
        if(add) return edge.on_add;
        else return edge.on_remove;
    };
    
    auto related_set = [&](ArchetypeEdge &edge, size_t storage)
    {
        if(add) edge.on_add = storage;
        else edge.on_remove = storage;
    };
    
    auto inverse_related_set = [&](ArchetypeEdge &edge, size_t storage)
    {
        if(!add) edge.on_add = storage;
        else edge.on_remove = storage;
    };

    auto &archetype = archetypes[archetype_idx];       
    auto &related = archetype.related_archetypes;
    
    auto related_it = related.find(cid);

    if(related_it != related.end() && related_get(related_it->second) != (size_t)(-1))
    {
        return archetypes[related_get(related_it->second)];
    }

    if(related_it == related.end())
    {
        related_it = related.insert({cid, ArchetypeEdge{}}).first;
    }

    auto dcid = detail::dense_component_type(cid);

#ifndef NDEBUG
    if(add && archetype.archetype.test(dcid))
    {
        std::cerr << "Attempt to add a component to an entity which already has an instance of it\n";
        std::abort();
    }

    if(!add && !archetype.archetype.test(dcid))
    {
        std::cerr << "Attempt to remove a component from an entity which does not have an instance of it\n";
        std::abort();
    }
#endif

    auto merged = archetype.archetype;
    if(add)
    {
        merged.set(dcid);
    }
    else 
    {
        merged.unset(dcid);
    }

    if(auto it = archetype_map.find(merged); it != archetype_map.end())
    {
        related_set(related_it->second, it->second);
        return archetypes[it->second];
    }

    auto inst = ArchetypeStorage{};
    auto edge = ArchetypeEdge{};
    inverse_related_set(edge, archetype_idx);

    inst.archetype = std::move(merged);
    inst.related_archetypes[cid] = edge;

    for(auto &[component, vec] : archetype.storage)
    {
        if(!add && component == cid)
        {
            continue;
        }

        inst.storage[component] = {vec.get_element_size(), vec.get_descriptor()};
    }
    
    if(add)
    {
        if(cid.packed_size() > 0)
        {
            inst.storage[cid] = dib::structures::ErasedVec{ cid.packed_size(), cid };
        }
    }

    auto result = register_archetype(std::move(inst));
    related_set(archetypes[archetype_idx].related_archetypes[cid], result);

    return archetypes[result];
}

uint64_t Entities::alloc_entity()
{
    if(free_ids.empty())
    {
        entity_map.emplace_back();
        return entity_map.size() - 1;
    }

    auto free = free_ids.back();
    free_ids.pop_back();

    return free;
}

uint64_t Entities::alloc_in_arch(ArchetypeStorage &storage)
{
    if(storage.free_indices.empty())
    {
        for(auto &[_, vec] : storage.storage)
        {
            vec.alloc_back();
        }
        storage.entity_ids.emplace_back();

        return storage.capacity++;
    }

    auto free = storage.free_indices.back();
    storage.free_indices.pop_back();

    return free;
}

void Entities::dealloc_entity(uint64_t index)
{
    entity_map[index].index = INVALID_ID;
    entity_map[index].version++;
    free_ids.push_back(index);
}

bool Entities::has_component(EntityID id, types::TypeDescriptor cid) const
{
    return archetypes[entity_map[id.index].archetype].archetype.test(detail::dense_component_type(cid));
}

EntityID Entities::create_entity()
{
    auto entity_id = alloc_entity();
    auto &entry = entity_map[entity_id];
    
    entry.archetype = 0;
    entry.index = alloc_in_arch(archetypes[0]);

    return {entity_id, entry.version};
}

void Entities::add_component(EntityID id, types::TypeDescriptor cid, dib::structures::ErasedPtr value)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];

    auto &new_arch = modify_archetype(cid, entry.archetype, true);
    auto &arch = archetypes[entry.archetype];

    auto new_pos = alloc_in_arch(new_arch);

    arch.free_indices.push_back(entry.index);
    arch.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[cid, vec] : arch.storage)
    {
        auto value = vec.inplace_take(entry.index);
        new_arch.storage[cid].uninitialized_assign(new_pos, value);
    }

    new_arch.storage[cid].uninitialized_assign(new_pos, value);
    new_arch.entity_ids[new_pos] = id;
    
    entry.archetype = new_arch.index;
    entry.index = new_pos;
}

void Entities::remove_component(EntityID id, types::TypeDescriptor cid)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];
    auto &new_arch = modify_archetype(cid, entry.archetype, false);
    auto &arch = archetypes[entry.archetype];

    auto new_pos = alloc_in_arch(new_arch);

    arch.free_indices.push_back(entry.index);
    arch.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[ocid, vec] : arch.storage)
    {
        if(cid == ocid)
        {
            vec.inplace_destruct(entry.index);
            continue;
        }

        auto value = vec.inplace_take(entry.index);
        new_arch.storage[ocid].uninitialized_assign(new_pos, value);
    }

    new_arch.entity_ids[new_pos] = id;
    
    entry.archetype = new_arch.index;
    entry.index = new_pos;
}

void Entities::destroy_entity(EntityID id)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];
    auto &storage = archetypes[entry.archetype];

    storage.free_indices.push_back(entry.index);
    storage.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[_, vec] : storage.storage)
    {
        vec.inplace_destruct(entry.index);
    }

    dealloc_entity(id.index);
}

// Commands //
struct DestroyEntityHeader
{
    EntityID id;
};

struct CreateEntityHeader
{
    uint8_t component_count;
};

struct AddComponentHeader
{
    EntityID entity;
};

struct RemoveComponentHeader
{
    EntityID entity;
    types::TypeDescriptor id;
};

void Commands::push_create_entity(uint8_t component_count)
{
    CreateEntityHeader header;
    header.component_count = component_count;

    entries.push(header);
}

void Commands::push_destroy_entity(EntityID entity)
{
    DestroyEntityHeader header;
    header.id = entity;

    entries.push(header);
}

void Commands::push_remove_component(EntityID entity, types::TypeDescriptor id)
{
    RemoveComponentHeader header;
    header.entity = entity;
    header.id = id;

    entries.push(header);
}

void Commands::push_add_component(EntityID entity)
{
    AddComponentHeader header;
    header.entity = entity;

    entries.push(header);
}

void Commands::flush()
{
    while(entries.size() > 0)
    {
        if(entries.top_type() == types::typedesc<AddComponentHeader>)
        {
            auto entry = entries.top_as<AddComponentHeader>();
            entries.pop();

            storage->add_component(entry.entity, entries.top_type(), entries.top());
            entries.pop_relocated();
        }
        else if(entries.top_type() == types::typedesc<RemoveComponentHeader>)
        {
            auto entry = entries.top_as<RemoveComponentHeader>();
            entries.pop();

            storage->remove_component(entry.entity, entry.id);
        }
        else if(entries.top_type() == types::typedesc<DestroyEntityHeader>)
        {
            auto entry = entries.top_as<DestroyEntityHeader>();
            entries.pop();

            storage->destroy_entity(entry.id);
        }
        else if(entries.top_type() == types::typedesc<CreateEntityHeader>)
        {
            auto component_count = entries.top_as<CreateEntityHeader>().component_count;
            entries.pop();
            auto id = storage->create_entity();
                
            for(size_t i = 0; i < component_count; i++)
            {
                storage->add_component(id, entries.top_type(), entries.top());
                entries.pop_relocated();
            }
        }
    }
    
    assert(entries.size() == 0);
}

// BasicQueryIterator //
void BasicQueryIterator::advance_to_valid()
{
    if(arch_it >= arch->size())
        return;
    
    if(place_it == storage().capacity)
    {
        arch_it++;
        place_it = 0;
        
        if(arch_it >= arch->size())
            return;
    }
            
    while(storage().entity_ids[place_it].index == INVALID_ID)
    {
        place_it++;

        if(place_it == storage().capacity)
        {
            arch_it++;
            place_it = 0;

            if(arch_it >= arch->size())
                return;
        }
    }
}

void BasicQueryIterator::advance()
{
    place_it++;
    advance_to_valid();
}

BasicQueryIterator::BasicQueryIterator(Entities *world, const std::vector<size_t> &arch)
{
    this->world = world;
    this->arch = &arch;

    arch_it = 0;
    place_it = 0;
    
    advance_to_valid();
}

BasicQueryIterator::BasicQueryIterator(End_t, Entities *world, const std::vector<size_t> &arch)
{
    this->world = world;
    this->arch = &arch;

    arch_it = arch.size();
    place_it = 0;
}