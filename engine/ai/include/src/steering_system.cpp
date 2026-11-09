//
// Created by p.pagliazzo on 09/11/2026.
//

#include "../systems/steering_system.h"
#include "../components/steering_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include <iostream>

void CameraSystem::onUpdate(Registry& registry, float dt)
{
    registry.view<SteeringComponent>([&](Entity e, SteeringComponent& steeringComponent)
    {
        if (abs(steeringComponent.direction.x) + abs(steeringComponent.direction.y) == 1.f ||abs(steeringComponent.direction.x) + abs(steeringComponent.direction.y) == 0.f)
        {
            if (registry.hasComponent<Transform2D>(e))
            {

            } else {
                std::cerr << "[steering] le composent steering_component ne peut pas exister sur une Entity qui na pas de Transform2D" << std::endl;
            }
        }
        else
        {
            std::cerr << "[steering] le composent steering_component a un direction non vallide les valeur doive etre compris entre 0 et 1" << std::endl;
        }
    });
}