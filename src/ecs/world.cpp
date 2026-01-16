#include "dib/ecs/world.h"
#include "dib/debug.h"

#include <unordered_map>
#include <iostream>

using namespace dib::ecs;
namespace types = dib::types;

bool EntityID::is_invalid() const
{
    return index == INVALID_ID;
}

// Archetypes //
ArchetypeStorage::~ArchetypeStorage()
{
    for(size_t i = 0; i < capacity; i++)
    {
        for(auto &elem : free_indices)
        {
            if(elem == i) goto end_of_loop;
        }
        
        for(auto &[_, vec] : component_stores)
        {
            vec.inplace_destruct(i);
        }

        end_of_loop:;
    }

    for(auto &[_, vec] : component_stores)
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
    auto index = archetype_stores.size();
    archetype_stores.push_back(std::move(in));

    auto &archetype = archetype_stores.back();
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

    for(size_t i = 0; i < archetype_stores.size(); i++)
    {
        if(in.is_subset_of(archetype_stores[i].archetype))
        {
            matching.push_back(i);
        }
    }

    query_map[in] = std::move(matching);
}

ArchetypeStorage &Entities::add_or_remove_from_archetype(types::TypeDescriptor cid, ArchetypeID archetype_id, bool add) 
{
    auto related_get = [&](const ArchetypeEdge &edge)
    {
        if(add) return edge.on_add;
        else return edge.on_remove;
    };
    
    auto related_set = [&](ArchetypeEdge &edge, ArchetypeID id)
    {
        if(add) edge.on_add = id;
        else edge.on_remove = id;
    };
    
    auto inverse_related_set = [&](ArchetypeEdge &edge, ArchetypeID id)
    {
        if(!add) edge.on_add = id;
        else edge.on_remove = id;
    };

    auto &archetype = archetype_stores[archetype_id];       
    auto &related = archetype.related_archetypes;
    
    auto related_it = related.find(cid);

    if(related_it != related.end() && related_get(related_it->second) != (ArchetypeID)(-1))
    {
        return archetype_stores[related_get(related_it->second)];
    }

    if(related_it == related.end())
    {
        related_it = related.insert({cid, ArchetypeEdge{}}).first;
    }

    auto dcid = detail::dense_component_type(cid);

    if(add && archetype.archetype.test(dcid))
    {
        RUNTIME_ERROR("Attempt to add a component to an entity which already has an instance of it\n");
    }

    if(!add && !archetype.archetype.test(dcid))
    {
        RUNTIME_ERROR("Attempt to remove a component from an entity which does not have an instance of it\n");
    }

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
        return archetype_stores[it->second];
    }

    auto inst = ArchetypeStorage{};
    auto edge = ArchetypeEdge{};
    inverse_related_set(edge, archetype_id);

    inst.archetype = std::move(merged);
    inst.related_archetypes[cid] = edge;

    for(auto &[component, vec] : archetype.component_stores)
    {
        if(!add && component == cid)
        {
            continue;
        }

        inst.component_stores[component] = {vec.get_element_size(), vec.get_descriptor()};
    }
    
    if(add)
    {
        if(cid.packed_size() > 0)
        {
            inst.component_stores[cid] = dib::structures::ErasedVec{ cid.packed_size(), cid };
        }
    }

    auto result = register_archetype(std::move(inst));
    related_set(archetype_stores[archetype_id].related_archetypes[cid], result);

    return archetype_stores[result];
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
        for(auto &[_, vec] : storage.component_stores)
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
    return archetype_stores[entity_map[id.index].archetype].archetype.test(detail::dense_component_type(cid));
}

EntityID Entities::create_entity()
{
    auto entity_id = alloc_entity();
    auto &entry = entity_map[entity_id];
    
    entry.archetype = 0;
    entry.index = alloc_in_arch(archetype_stores[0]);

    return {entity_id, entry.version};
}

void Entities::add_component_raw(EntityID id, types::TypeDescriptor cid, dib::structures::ErasedPtr value)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];

    auto &new_arch = add_or_remove_from_archetype(cid, entry.archetype, true);
    auto &arch = archetype_stores[entry.archetype];

    auto new_pos = alloc_in_arch(new_arch);

    arch.free_indices.push_back(entry.index);
    arch.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[cid, vec] : arch.component_stores)
    {
        auto value = vec.inplace_take(entry.index);
        new_arch.component_stores[cid].uninitialized_assign(new_pos, value);
    }

    new_arch.component_stores[cid].uninitialized_assign(new_pos, value);
    new_arch.entity_ids[new_pos] = id;
    
    entry.archetype = new_arch.index;
    entry.index = new_pos;
}

void Entities::remove_component(EntityID id, types::TypeDescriptor cid)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];
    auto &new_arch = add_or_remove_from_archetype(cid, entry.archetype, false);
    auto &arch = archetype_stores[entry.archetype];

    auto new_pos = alloc_in_arch(new_arch);

    arch.free_indices.push_back(entry.index);
    arch.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[ocid, vec] : arch.component_stores)
    {
        if(cid == ocid)
        {
            vec.inplace_destruct(entry.index);
            continue;
        }

        auto value = vec.inplace_take(entry.index);
        new_arch.component_stores[ocid].uninitialized_assign(new_pos, value);
    }

    new_arch.entity_ids[new_pos] = id;
    
    entry.archetype = new_arch.index;
    entry.index = new_pos;
}

void Entities::destroy_entity(EntityID id)
{
    assert_exists(id);

    auto &entry = entity_map[id.index];
    auto &storage = archetype_stores[entry.archetype];

    storage.free_indices.push_back(entry.index);
    storage.entity_ids[entry.index].index = INVALID_ID;
    for(auto &[_, vec] : storage.component_stores)
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
    while(entries.size_bytes() > 0)
    {
        if(entries.top().type == types::typedesc<AddComponentHeader>)
        {
            auto entry = entries.top_as<AddComponentHeader>();
            entries.pop();

            storage->add_component_raw(entry.entity, entries.top().type, entries.top().pointer);
            entries.pop_nondestructive();
        }
        else if(entries.top().type == types::typedesc<RemoveComponentHeader>)
        {
            auto entry = entries.top_as<RemoveComponentHeader>();
            entries.pop();

            storage->remove_component(entry.entity, entry.id);
        }
        else if(entries.top().type == types::typedesc<DestroyEntityHeader>)
        {
            auto entry = entries.top_as<DestroyEntityHeader>();
            entries.pop();

            storage->destroy_entity(entry.id);
        }
        else if(entries.top().type == types::typedesc<CreateEntityHeader>)
        {
            auto component_count = entries.top_as<CreateEntityHeader>().component_count;
            entries.pop();
            auto id = storage->create_entity();
                
            for(size_t i = 0; i < component_count; i++)
            {
                storage->add_component_raw(id, entries.top().type, entries.top().pointer);
                entries.pop_nondestructive();
            }
        }
    }
    
    ASSERT(entries.size_bytes() == 0);
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
        
        if(!arch || arch_it >= arch->size())
            return;
    }
            
    while(storage().entity_ids[place_it].index == INVALID_ID)
    {
        place_it++;

        if(place_it == storage().capacity)
        {
            arch_it++;
            place_it = 0;

            if(!arch || arch_it >= arch->size())
                return;
        }
    }
}

void BasicQueryIterator::advance()
{
    place_it++;
    advance_to_valid();
}

BasicQueryIterator::BasicQueryIterator(Entities *world, const std::vector<ArchetypeID> &arch)
{
    this->entities = world;
    this->arch = &arch;

    arch_it = 0;
    place_it = 0;
    
    advance_to_valid();
}

BasicQueryIterator::BasicQueryIterator(End_t, Entities *world, const std::vector<ArchetypeID> &arch)
{
    this->entities = world;
    this->arch = &arch;

    arch_it = arch.size();
    place_it = 0;
}

BasicQueryIterator::BasicQueryIterator(Entities *world, ArchetypeID singlet)
{
    this->entities = world;
    this->arch = nullptr;
    this->singlet_arch = singlet;

    arch_it = 0;
    place_it = 0;
    
    advance_to_valid();
}

BasicQueryIterator::BasicQueryIterator(End_t, Entities *world, ArchetypeID arch)
{
    this->entities = world;
    this->arch = nullptr;
    this->singlet_arch = arch;

    arch_it = 1;
    place_it = 0;
}