#include "../include/ecs.h"
#include "../include/umath.h"

#include "assert.h"

using namespace dib;

struct Person{};
struct Position{
    math::float3 value;
    constexpr Position(math::float3 value) : value(value) {}
};
struct Dog{};

int main()
{
    ecs::Scene scene;

    auto id = scene.create_entity();
    scene.add_component<Person>(id);
    scene.add_component<Position>(id, math::float3{1, 2, 3});

    for(auto [id, _, pos] : scene.query<Person, const Position>())
    {
        std::cout << id.index << std::endl;
    }

    scene.remove_component<Person>(id);
    
    for(auto [id, _, pos] : scene.query<Person, const Position>())
    {
        std::cout << id.index << std::endl;
    }
}