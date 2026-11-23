//
// Created by p.pagliazzo on 23/11/2026.
//

#include "../include/systems/steeringFlee_system.h"
#include "../include/components/steering_component.h"
#include "components/steeringFlee_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include <cmath>


void SteeringFleeSystem::onStart(Registry& registry)
{

}

void SteeringFleeSystem::onUpdate(Registry& registry, float dt)
{
    registry.view<SteeringFleeComponent>([&](Entity e, SteeringFleeComponent& steeringFleeComponent)
    {
        if (registry.hasComponent<SteeringComponent>(e))
        {
            if (registry.hasComponent<Transform2D>(e))
            {
                if (steeringFleeComponent.isActif)
                {
                    if (steeringFleeComponent.listeCible != nullptr)
                    {
                        sf::Vector2f pos = registry.getComponent<Transform2D>(e).getPosition();
                        auto& steeringCoponent = registry.getComponent<SteeringComponent>(e);
                        SteeringFleeComponent::Cible* temp = steeringFleeComponent.listeCible;
                        sf::Vector2f direction = steeringCoponent.direction;
                        if (direction.x == 0 && direction.y == 0)
                        {
                            do
                            {
                                sf::Vector2f evitablePos = temp->position;
                                evitablePos.x -= pos.x;
                                evitablePos.y -= pos.y;
                                float norme = evitablePos.x*evitablePos.x + evitablePos.y*evitablePos.y;
                                norme = sqrt(norme);

                                if (temp->distanfceAdaptable)
                                {
                                    if (evitablePos.x != 0 || evitablePos.y != 0)
                                    {
                                        if (norme < temp->distanfceAdaptableDebut)
                                        {
                                            direction.x += evitablePos.x * (temp->distanfceAdaptableDebut / norme);
                                            direction.y += evitablePos.y * (temp->distanfceAdaptableDebut / norme);
                                        }
                                    }
                                }else{
                                    direction += evitablePos;
                                }
                                if (temp->next != nullptr)temp = temp->next;
                            }while (temp->next != nullptr );
                            if (direction.x == 0 && direction.y == 0)
                            {
                                float norme = direction.x*direction.x + direction.y*direction.y;
                                norme = sqrt(norme);
                                direction.x /= norme;
                                direction.y /= norme;
                                if (norme > 100.f)
                                {
                                    steeringCoponent.rapidite = 1.f;
                                }else{
                                    steeringCoponent.rapidite = norme/100.f;
                                }
                                steeringCoponent.direction = direction;
                            }
                        }else{
                            do
                            {

                                sf::Vector2f evitablePos = temp->position;
                                evitablePos.x -= pos.x;
                                evitablePos.y -= pos.y;
                                float norme = evitablePos.x*evitablePos.x + evitablePos.y*evitablePos.y;
                                norme = sqrt(norme);
                                evitablePos.x /= norme;
                                evitablePos.y /= norme;

                                float degres = evitablePos.x*steeringCoponent.direction.x + evitablePos.y*steeringCoponent.direction.y;
                                degres = std::acosf(degres);

                                if (temp->distanfceAdaptable)
                                {
                                    if (temp->distanfceAdaptableDebut < norme)
                                    {
                                        if (evitablePos.x != 0 || evitablePos.y != 0)
                                        {
                                            if (norme < temp->distanfceAdaptableDebut)
                                            {
                                                float degresAdapter = temp->angle * (temp->distanfceAdaptableDebut / norme);
                                                if (abs(degres) < degresAdapter)
                                                {
                                                    direction.x += evitablePos.x;
                                                    direction.y += evitablePos.y;
                                                }else
                                                {
                                                    float rad = degresAdapter * 3.14159265f / 180.0f;
                                                    float c = std::cosf(rad);
                                                    float s = std::sinf(rad);
                                                    direction.x += steeringCoponent.direction.x * c - steeringCoponent.direction.y * s;
                                                    direction.y += steeringCoponent.direction.x * s + steeringCoponent.direction.y * c;
                                                }
                                            }
                                        }
                                    }
                                }else{
                                    if (abs(degres) < temp->angle)
                                    {
                                        direction.x += evitablePos.x;
                                        direction.y += evitablePos.y;
                                    }else{
                                        float rad = degres * 3.14159265f / 180.0f;
                                        float c = std::cosf(rad);
                                        float s = std::sinf(rad);
                                        direction.x += steeringCoponent.direction.x * c - steeringCoponent.direction.y * s;
                                        direction.y += steeringCoponent.direction.x * s + steeringCoponent.direction.y * c;
                                    }
                                }
                                if (temp->next != nullptr)temp = temp->next;
                            }while (temp->next != nullptr );
                        }
                        float norme = direction.x*direction.x + direction.y*direction.y;
                        norme = sqrt(norme);
                        direction.x /= norme;
                        direction.y /= norme;
                    }
                }
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
