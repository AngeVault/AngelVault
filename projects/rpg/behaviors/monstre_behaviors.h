#pragma once
#include "ecs/behavior.h"
#include "ecs/behavior_factory.h"
#include "ecs/registry.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include "components/collider2d.h"
#include "components/transform2d.h"
#include "../components/life.h"
#include "../components/npc.h"
#include "input/input.h"
#include <math.h>

#include "math/interpolation.h"

// Comportement « monstre ».
// Après compilation, glissez ce fichier sur une entité (Hierarchy, Inspector ou vue scène),
// ou Inspector > Ajouter un composant > Comportement > monstre.
class monstre : public IBehavior {
public:
    // Réglages visibles dans l'Inspector et enregistrés dans la scène (float, int, bool, sf::Vector2f, std::string)
    PROPERTY(float, speed, 100.f)

    // Appelé une fois, avant le premier onUpdate
    void onStart(Entity self, Registry& registry) override {
    }

    // Appelé à chaque frame (dt en secondes)
    void onUpdate(Entity self, Registry& registry, float dt) override {
        auto& life = registry.getComponent<Life>(self);
        if (life.actualLife > 0)
        {
            auto& npc = registry.getComponent<Npc>(self);
            if (npc.ennemies)
            {

                registry.view<NameComponent>([&](Entity e, NameComponent& name)
                {
                    if (name.name == "Player")
                    {
                        auto& rbSelf = registry.getComponent<RigidBody2D>(self);
                        auto& coSelf = registry.getComponent<Collider2D>(self);
                        auto& coCible = registry.getComponent<Collider2D>(e);
                        auto& trSelf = registry.getComponent<Transform2D>(self);
                        auto& trCible = registry.getComponent<Transform2D>(e);
                        sf::Vector2f posSelf = trSelf.getPosition();
                        sf::Vector2f posCible = trCible.getPosition();
                        posSelf.x += coSelf.offset.x;
                        posSelf.y += coSelf.offset.y;
                        posCible.x += coCible.offset.x;
                        posCible.y += coCible.offset.y;
                        if (((posSelf.x * posCible.x) + (posSelf.y * posCible.y)) < npc.vision * npc.vision )
                        {
                            float moveX = 0;
                            float moveY = 0;
                            if ( fabsf(posSelf.x - posCible.x) > 50.f || fabsf(posSelf.y - posCible.y) > 50.f)
                            {


                                if (posSelf.x - posCible.x > 50.f)
                                {
                                    moveX = -1.f;
                                }else if (posCible.x - posSelf.x > 50.f)
                                {
                                    moveX = 1.f;
                                }
                                if (posSelf.y - posCible.y > 50.f)
                                {
                                    moveY = -1.f;
                                }else if (posCible.y - posSelf.y > 50.f)
                                {
                                    moveY = 1.f;
                                }

                                if (moveX != 0.f && moveY != 0.f){ moveX /= 2.f; moveY /= 2.f; }

                            }
                            rbSelf.velocity.x = moveX * speed;
                            rbSelf.velocity.y = moveY * speed;
                        }
                    }else
                    {
                        //auto& lifeCible = registry.getComponent<Life>(e);
                        //lifeCible.actualLife -= 50*dt;
                    }
                });

            }
        }
    }
};

// Nom utilisé dans les fichiers .scene / .prefab
REGISTER_BEHAVIOR(monstre)
