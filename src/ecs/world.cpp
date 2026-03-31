#include "dib/ecs/world.h"
#include "dib/debug.h"
#include "dib/ecs/entities.h"
#include "dib/bijection.h"
#include "dib/iou.h"
#include "dib/debug.h"

#include <unordered_map>

using namespace dib::ecs;
namespace refl = dib::refl;

// Component type mapping functions //
static dib::IOU<dib::structures::Bijection<refl::Type, ComponentID>> component_id_map;
static ComponentID::ValueType &next_free_component_id()
{
    static ComponentID::ValueType value;
    return value;
}

ComponentID detail::component_id(refl::Type desc)
{
    auto it = component_id_map.value().find(desc);
    if (it != component_id_map.value().end()) return it->second;

    auto new_id = next_free_component_id()++;
    component_id_map.value().insert(desc, { new_id });
    
    LOGF("Component {} being assigned id of {}", desc.name(), new_id);

    return { new_id };
}

refl::Type ComponentID::type() const
{
    if(auto type = component_id_map.value().find(*this); type != component_id_map.value().end())
    {
        return type->first;
    }
    
    RUNTIME_ERROR("No component type descriptor associated with id {}", value());
}

bool Entity::is_invalid() const
{
    return index == INVALID_ID;
}

// Archetype //
Archetype::Archetype(dib::structures::SparseBitset &&bits)
    : _bits(MOVE(bits))
{}

Archetype Archetype::from_bits(dib::structures::SparseBitset &&bits)
{
    return { MOVE(bits) };
}

const dib::structures::SparseBitset &Archetype::bits() const 
{ 
    return _bits; 
}

bool Archetype::has_component(ComponentID id) const 
{ 
    return _bits.test(id.value()); 
}

bool Archetype::has_component(refl::Type id) const 
{ 
    return has_component(detail::component_id(id)); 
}

bool Archetype::is_subset_of(const Archetype &other) const 
{ 
    return _bits.is_subset_of(other._bits); 
}

// ArchetypeStorage //
detail::ArchetypeStorage::ArchetypeStorage()
    : index(0), capacity(0)
{}

detail::ArchetypeStorage::ArchetypeStorage(ArchetypeStorage &&other)
    : archetype(MOVE(other.archetype)), index(other.index), 
    component_stores(MOVE(other.component_stores)), entity_ids(MOVE(other.entity_ids)),
    free_indices(MOVE(other.free_indices)), capacity(other.capacity),
    related_archetypes(MOVE(other.related_archetypes))
{
    other.capacity = 0;
}

detail::ArchetypeStorage::~ArchetypeStorage()
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

void detail::ArchetypeStorage::create_storage_for(ComponentID cid)
{
    auto type = cid.type();

    if(type.packed_size() != 0)
    {
        component_stores[cid] = 
            dib::structures::ErasedVec{ type.packed_size(), type };
    }
}

// World //
Entities::Entities()
{
    register_archetype({});
}

size_t Entities::register_archetype(detail::ArchetypeStorage &&in)
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

    query_map[in] = MOVE(matching);
}

detail::ArchetypeStorage &Entities::add_or_remove_from_archetype(
    ComponentID cid, detail::ArchetypeID archetype_id, bool add) 
{
    // Helper lambdas to get and set the related archetype edges based on the 'add' variable
    auto related_get = [&](const detail::ArchetypeEdge &edge)
    {
        if(add) return edge.on_add;
        else return edge.on_remove;
    };
    
    auto related_set = [&](detail::ArchetypeEdge &edge, detail::ArchetypeID id)
    {
        if(add) edge.on_add = id;
        else edge.on_remove = id;
    };
    
    auto inverse_related_set = [&](detail::ArchetypeEdge &edge, detail::ArchetypeID id)
    {
        if(!add) edge.on_add = id;
        else edge.on_remove = id;
    };

    // Get the archetype being operated on.
    auto &archetype = archetype_stores[archetype_id];       
    auto &related = archetype.related_archetypes;
    
    auto related_it = related.find(cid);

    // If exists in cache, return from cache.
    if(related_it != related.end() && related_get(related_it->second) != (detail::ArchetypeID)(-1))
    {
        return archetype_stores[related_get(related_it->second)];
    }

    // If our edge cache is truly blank (neither add or remove exist),
    // add to it, since this operation must create a new cache entry.
    if(related_it == related.end())
    {
        related_it = related.insert({cid, detail::ArchetypeEdge{}}).first;
    }

    if(add && archetype.archetype.has_component(cid))
    {
        RUNTIME_ERROR(
            "Attempt to add a {} to an entity which already has an instance of it",
            cid.type().name());
    }

    if(!add && !archetype.archetype.has_component(cid))
    {
        RUNTIME_ERROR(
            "Attempt to remove {} from an entity which does not have an instance of it",
            cid.type().name());
    }

    // Calculate the newly formed archetype
    auto merged_bits = archetype.archetype.bits();
    if(add)
    {
        merged_bits.set(cid.value());
    }
    else 
    {
        merged_bits.unset(cid.value());
    }

    auto merged = Archetype::from_bits(MOVE(merged_bits));

    // If our new archetype already exists in the scene,
    // return its storage and cache its relationship to the
    // provided archetype.
    if(auto it = archetype_map.find(merged); it != archetype_map.end())
    {
        related_set(related_it->second, it->second);
        return archetype_stores[it->second];
    }

    // Build a new archetype storage from the new archetype.
    auto inst = detail::ArchetypeStorage{};
    auto edge = detail::ArchetypeEdge{};
    inverse_related_set(edge, archetype_id);

    inst.archetype = MOVE(merged);
    inst.related_archetypes[cid] = edge;

    for(auto &[component, vec] : archetype.component_stores)
    {
        if(!add && component == cid)
        {
            continue;
        }

        inst.create_storage_for(cid);
    }
    
    if(add)
    {
        inst.create_storage_for(cid);
    }

    auto result = register_archetype(MOVE(inst));
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

uint64_t Entities::alloc_in_arch(detail::ArchetypeStorage &storage)
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

bool Entities::has_component(Entity id, ComponentID cid) const
{
    assert_exists(id);
    return archetype_stores[entity_map[id.index].archetype].archetype.has_component(cid);
}

void Entities::assert_exists(Entity id) const
{
    if(entity_map.size() <= id.index || entity_map[id.index].version != id.version)
    {
        RUNTIME_ERROR("Invalid access to entity with id {}.", auto(id.index));
    }
}

void Entities::assert_has_component(Entity id, ComponentID cid) const
{
    if(!has_component(id, cid))
    {
        RUNTIME_ERROR("Attempt to access a component of type {} which does not exist.", cid.type().name());
    }
}

bool Entities::is_alive(Entity id) const
{
    return entity_map.size() > id.index && entity_map[id.index].version == id.version;
}

Entity Entities::create_entity()
{
    auto index = alloc_entity();
    auto &entry = entity_map[index];
    
    entry.archetype = 0;
    entry.index = alloc_in_arch(archetype_stores[0]);

    auto entity_id = Entity(index, entry.version);
    archetype_stores[0].entity_ids[entry.index] = entity_id;

    return entity_id;
}

Entity Entities::create_uninitialized_entity(const Archetype &archetype)
{
    detail::ArchetypeID archetype_id;

    if(auto it = archetype_map.find(archetype); it != archetype_map.end())
    {
        archetype_id = it->second;
    }
    else
    {
        auto new_storage = detail::ArchetypeStorage{};
        new_storage.archetype = archetype;
        
        for(auto cid : new_storage.archetype.bits())
        {
            new_storage.create_storage_for(
                ComponentID(cid));
        }

        archetype_id = register_archetype(MOVE(new_storage));
    }
    
    auto index = alloc_entity();
    auto &entry = entity_map[index];

    entry.archetype = archetype_id;
    entry.index = alloc_in_arch(archetype_stores[archetype_id]);

    auto entity_id = Entity(index, entry.version);
    archetype_stores[archetype_id].entity_ids[entry.index] = entity_id;

    return entity_id;
}

void *Entities::get_component_raw(Entity id, ComponentID cid)
{
    assert_exists(id);
    assert_has_component(id, cid);
    
    if(cid.type().packed_size() == 0)
    {
        return mem::pointer_to_zst<void>();
    }
    
    auto &entry = entity_map[id.index];
    auto &storage = archetype_stores[entry.archetype];
    auto &vec = storage.component_stores[cid];

    return vec.pointer(entry.index);
}

const void *Entities::get_component_raw(Entity id, ComponentID cid) const
{
    return const_cast<Entities*>(this)->get_component_raw(id, cid);
}

void Entities::add_component_raw(Entity id, ComponentID cid, dib::structures::ErasedPtr value)
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

void Entities::remove_component(Entity id, ComponentID cid)
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

void Entities::destroy_entity(Entity id)
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
void Commands::flush()
{
    while(entries.size_bytes() > 0)
    {
        auto command = entries.top_as<CommandPtr>();
        entries.pop();

        command(this);
    }
    
    ASSERT(entries.size_bytes() == 0);
}