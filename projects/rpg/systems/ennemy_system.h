#pragma once
#include "game_systems.h"
#include "ecs/registry.h"
#include "../components/npc.h"
#include "../components/stats.h"
#include "../components/life.h"
#include "components/steering_component.h"
#include "components/steeringSeek_component.h"
#include "components/steeringFlee_component.h"
#include "components/transform2d.h"
#include "components/rigid_body2d.h"
#include <iostream>

// Système de jeu « ennemy_system » : ajouté automatiquement au jeu après compilation.
// Activer / désactiver : éditeur > Exécuter > Systèmes...
class ennemy_system : public ISystem {
public:
    ennemy_system() { priority = 6; }   // ordre : physique 0, collisions 1, scripts 2, ... rendu 10
    // Pour dessiner : un constructeur ennemy_system(sf::RenderWindow& window) est appelé à la place.

    void onStart(Registry& registry) override
    {

    };

    void onUpdate(Registry& registry, float dt) override
    {
        registry.view<Npc>([&](Entity e, Npc& npc)
        {
            if (npc.ennemies)
            {
                if (registry.hasComponent<Stats>(e))
                {
                    if (registry.hasComponent<Life>(e))
                    {
                        if (registry.hasComponent<Transform2D>(e))
                        {
                            if (registry.hasComponent<RigidBody2D>(e))
                            {
                                if (registry.hasComponent<SteeringComponent>(e))
                                {
                                    Life life = registry.getComponent<Life>(e);
                                    SteeringComponent steering = registry.getComponent<SteeringComponent>(e);
                                    Stats stats = registry.getComponent<Stats>(e);
                                    if (steering.rapidite != 0.f)
                                    {
                                        if (life.actualLife > 0.f)
                                        {

                                            RigidBody2D& rigidBody = registry.getComponent<RigidBody2D>(e);

                                            if ( rigidBody.velocity.x > 0) rigidBody.velocity.x = rigidBody.velocity.x - stats.agilite*2*dt*rigidBody.velocity.x;
                                            if ( rigidBody.velocity.x < 0) rigidBody.velocity.x = rigidBody.velocity.x + stats.agilite*2*dt*rigidBody.velocity.x*-1;
                                            if ( rigidBody.velocity.y > 0) rigidBody.velocity.y = rigidBody.velocity.y - stats.agilite*2*dt*rigidBody.velocity.y;
                                            if ( rigidBody.velocity.y < 0) rigidBody.velocity.y = rigidBody.velocity.y + stats.agilite*2*dt*rigidBody.velocity.y*-1;


                                            rigidBody.applyForce((steering.direction * ((stats.agilite*2)+stats.force))*steering.rapidite * dt*100000.f);


                                            rigidBody.mass = 60 + stats.constitution*2 ;
                                        }
                                    }
                                    registry.view<NameComponent,Transform2D,RigidBody2D>([&](Entity i, NameComponent& nameComponent, Transform2D& transform, RigidBody2D& rigidBody)
                                    {
                                        if (nameComponent.name == "Player")
                                        {
                                            sf::Vector2f distance = {0,0};
                                            sf::Vector2f pos = registry.getComponent<Transform2D>(e).getPosition();
                                            sf::Vector2f posi = transform.getPosition();
                                            distance.x = (pos.x - posi.x)*(pos.x - posi.x);
                                            distance.y = (pos.y - posi.y)*(pos.y - posi.y);
                                            if (sqrtf(distance.x + distance.y) < npc.vision)
                                            {
                                                registry.getComponent<SteeringSeekComponent>(e).positionCible = posi;
                                            }
                                            if (registry.hasComponent<Life>(i))
                                            {
                                                if (sqrtf(distance.x + distance.y) < 20)
                                                {
                                                    auto&  life = registry.getComponent<Life>(i);
                                                    life.actualLife -= stats.force*10*dt;
                                                }
                                            }
                                        }
                                    });
                                }
                            }
                            else
                            {
                                std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de RigidBody2D" << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Transform2D" << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Life" << std::endl;
                    }
                }
                else
                {
                    std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Stat" << std::endl;
                }
            }
            else
            {
                if (registry.hasComponent<Stats>(e))
                {
                    if (registry.hasComponent<Life>(e))
                    {
                        if (registry.hasComponent<Transform2D>(e))
                        {
                            if (registry.hasComponent<RigidBody2D>(e))
                            {
                                if (registry.hasComponent<SteeringComponent>(e))
                                {
                                    Life life = registry.getComponent<Life>(e);
                                    SteeringComponent steering = registry.getComponent<SteeringComponent>(e);
                                    Stats stats = registry.getComponent<Stats>(e);
                                    if (steering.rapidite != 0.f)
                                    {
                                        if (life.actualLife > 0.f)
                                        {

                                            RigidBody2D& rigidBody = registry.getComponent<RigidBody2D>(e);

                                            if ( rigidBody.velocity.x > 0) rigidBody.velocity.x = rigidBody.velocity.x - stats.agilite*2*dt*rigidBody.velocity.x;
                                            if ( rigidBody.velocity.x < 0) rigidBody.velocity.x = rigidBody.velocity.x + stats.agilite*2*dt*rigidBody.velocity.x*-1;
                                            if ( rigidBody.velocity.y > 0) rigidBody.velocity.y = rigidBody.velocity.y - stats.agilite*2*dt*rigidBody.velocity.y;
                                            if ( rigidBody.velocity.y < 0) rigidBody.velocity.y = rigidBody.velocity.y + stats.agilite*2*dt*rigidBody.velocity.y*-1;


                                            rigidBody.applyForce((steering.direction * ((stats.agilite*2)+stats.force))*steering.rapidite * dt*100000.f);


                                            rigidBody.mass = 60 + stats.constitution*2 ;
                                        }
                                    }
                                    registry.view<NameComponent,Transform2D,RigidBody2D>([&](Entity i, NameComponent& nameComponent, Transform2D& transform, RigidBody2D& rigidBody)
                                    {
                                        if (nameComponent.name == "Player")
                                        {
                                            sf::Vector2f distance = {0,0};
                                            sf::Vector2f pos = registry.getComponent<Transform2D>(e).getPosition();
                                            sf::Vector2f posi = transform.getPosition();
                                            distance.x = (pos.x - posi.x)*(pos.x - posi.x);
                                            distance.y = (pos.y - posi.y)*(pos.y - posi.y);
                                            auto& steeringFlee = registry.getComponent<SteeringFleeComponent>(e);
                                            if (sqrtf(distance.x + distance.y) < npc.vision)
                                            {

                                                if (steeringFlee.listeCible != nullptr)
                                                {
                                                    steeringFlee.listeCible->position = posi;
                                                }
                                                else
                                                {
                                                    steeringFlee.addCible(posi,false,300,40.f);
                                                }
                                            }
                                            else
                                            {
                                                if (steeringFlee.listeCible != nullptr)
                                                {
                                                    steeringFlee.substractCible(steeringFlee.listeCible->id);
                                                }
                                            }
                                            if (sqrtf(distance.x + distance.y) < 20)
                                            {
                                                if (registry.hasComponent<Life>(i))
                                                {
                                                    auto&  life = registry.getComponent<Life>(i);
                                                    life.actualLife -= stats.force*10*dt;
                                                }
                                            }
                                        }
                                    });
                                }
                            }
                            else
                            {
                                std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de RigidBody2D" << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Transform2D" << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Life" << std::endl;
                    }
                }
                else
                {
                    std::cerr << "[NPC] le composent npc ne peut etre mis sur une entity qui na pas de Stat" << std::endl;
                }
            }
        });
    };
};

REGISTER_SYSTEM(ennemy_system)
