//
// Created by p.pagliazzo on 09/11/2026.
//

#include "systems/steering_system.h"
#include "components/steering_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include <iostream>


void SteeringSystem::onStart(Registry& registry)
{
    registry.view<SteeringComponent>([&](Entity e, SteeringComponent& steeringComponent)
    {
        if (registry.hasComponent<Transform2D>(e))
        {

        }else
        {
            std::cerr << "[Steering] le composent steering ne peut etre mis sur une entity qui na pas de transphorme2d" << std::endl;
        }
    });
}

void SteeringSystem::onUpdate(Registry& registry, float dt)
{

    registry.view<SteeringComponent>([&](Entity e, SteeringComponent& steeringComponent)
    {
        if (registry.hasComponent<Transform2D>(e))
        {

        }else
        {
            std::cerr << "[Steering] le composent steering ne peut etre mis sur une entity qui na pas de transphorme2d" << std::endl;
        }
    });
}
