#include "dib/ecs/entities.h"
#include "dib/functional.h"

// Driver functions //
void dib::ecs::BasicQuery::for_each(
    dib::functional::FunctionRef<void(EntityID)> fn, bool sync) const
{
    auto &archetypes = entities->query_map[*archetype];
    auto arch_id = 0zu;
    auto entity = 0zu;

    auto storage = [&] -> auto && { return entities->archetype_stores[archetypes[arch_id]]; };

    while(arch_id < archetypes.size())
    {
        if(entity == storage().capacity)
        {
        ITERATE_ARCH:
            arch_id++;
            entity = 0;
            
            if(arch_id >= archetypes.size())
                break;
        }
             
        while(storage().entity_ids[entity].index == INVALID_ID)
        {
            entity++;

            if(entity == storage().capacity)
                goto ITERATE_ARCH;
        }

        auto id = storage().entity_ids[entity];

        if(sync)
        {
            fn(id);
        }
        else
        {
            LOG("Executing on thread");
            entities->thread_scheduler().execute(
                fn, id
            );
        }

        entity++;
    }
    
    if(!sync)
    {
        entities->thread_scheduler().wait_for_complete();
    }
}

void dib::ecs::BasicQuery::for_each_exact(
    dib::functional::FunctionRef<void(EntityID)> fn, bool sync) const
{
    auto arch_it = entities->archetype_map.find(*archetype);

    if(arch_it == entities->archetype_map.end())
        return;

    auto &arch = *arch_it;
    auto storage = [&] -> auto && { return entities->archetype_stores[arch.second]; };
    auto entity = 0zu;

    while(true)
    {
        while(storage().entity_ids[entity].index == INVALID_ID)
        {
            entity++;

            if(entity == storage().capacity)
                break;
        }

        auto id = storage().entity_ids[entity];

        if(sync)
        {
            fn(id);
        }
        else
        {
            entities->thread_scheduler().execute(
                fn, id
            );
        }
    }

    if(!sync)
    {
        entities->thread_scheduler().wait_for_complete();
    }
}

// External API //
void dib::ecs::BasicQuery::for_each(dib::functional::FunctionRef<void(EntityID)> fn) const { for_each(fn, false); }
void dib::ecs::BasicQuery::for_each_sync(dib::functional::FunctionRef<void(EntityID)> fn) const { for_each(fn, true); }
void dib::ecs::BasicQuery::for_each_exact(dib::functional::FunctionRef<void(EntityID)> fn) const { for_each_exact(fn, false); }
void dib::ecs::BasicQuery::for_each_exact_sync(dib::functional::FunctionRef<void(EntityID)> fn) const { for_each_exact(fn, true); }

size_t dib::ecs::BasicQuery::count_exact() const
{
    return entities->archetype_stores[entities->archetype_map[*archetype]].capacity 
         - entities->archetype_stores[entities->archetype_map[*archetype]].free_indices.size();
}
size_t dib::ecs::BasicQuery::count() const
{
    size_t out = 0;
    for (auto &set : entities->query_map.at(*archetype))
    {
        out += entities->archetype_stores[set].capacity 
             - entities->archetype_stores[set].free_indices.size();
    }
    return out;
}