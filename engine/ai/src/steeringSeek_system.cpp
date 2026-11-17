//
// Created by p.pagliazzo on 10/11/2026.
//
#include "../include/systems/steeringSeek_system.h"
#include "../include/components/steering_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include <iostream>

#include "components/steeringSeek_component.h"

void SteeringSeekSystem::onStart(Registry& registry)
{

}

void SteeringSeekSystem::onUpdate(Registry& registry, float dt)
{
    registry.view<SteeringSeekComponent>([&](Entity e, SteeringSeekComponent& steeringSeekComponent)
    {
        if (registry.hasComponent<SteeringComponent>(e))
        {
            if (registry.hasComponent<Transform2D>(e))
            {
                sf::Vector2f pos = registry.getComponent<Transform2D>(e).getPosition();
                auto& steeringCoponent = registry.getComponent<SteeringComponent>(e);
                pos.x = steeringSeekComponent.positionCible.x - pos.x;
                pos.y = steeringSeekComponent.positionCible.y - pos.y;
                float norme = pos.x*pos.x + pos.y*pos.y;


                if (pos.x != 0 || pos.y != 0)
                {
                    norme = sqrt(norme);
                    if (norme > 100.f)
                    {
                        steeringCoponent.rapidite = 1.f;
                    }else{
                        steeringCoponent.rapidite = norme/100.f;
                    }
                    pos.x /= norme;
                    pos.y /= norme;
                }

                steeringCoponent.direction.x = pos.x;
                steeringCoponent.direction.y = pos.y;

            }else
            {
                std::cerr << "[Steering] le composent steering ne peut etre mis sur une entity qui na pas de transphorme2d" << std::endl;
            }
        }else
        {
            std::cerr << "[Steering] le composent steeringSeek ne peut etre mis sur une entity qui na pas de steering" << std::endl;
        }
    });
}
