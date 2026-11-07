//
// Created by pagli on 07/11/2026.
//

#pragma once

#include "systems/cameraFollow_system.h"
#include <iostream>
#include "components/camera2d.h"
#include "components/cameraFollow.h"
#include "components/transform2d.h"
#include "components/rigid_body2d.h"
#include "math/interpolation.h"
#include "ecs/registry.h"

void CameraFollowSystem::onUpdate(Registry &registry, float dt)
{
    registry.view<CameraFollow>([&](Entity e, CameraFollow& cameraFollow) {
        if (registry.hasComponent<Camera2d>(e))
        {
            if (cameraFollow.targetLock != NULL_ENTITY)
            {
                if (registry.isValid(cameraFollow.targetLock)) {
                    if (registry.hasComponent<Transform2D>(e)) {

                        if (registry.hasComponent<Transform2D>(cameraFollow.targetLock))
                        {
                            sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
                            sf::Vector2f positionCible = registry.getComponent<Transform2D>(cameraFollow.targetLock).getPosition();
                            sf::Vector2f velocity = {};
                            if (registry.hasComponent<RigidBody2D>(cameraFollow.targetLock))
                            {
                                velocity = registry.getComponent<RigidBody2D>(cameraFollow.targetLock).velocity;
                            } else {
                                std::cerr << " [Camera] l'Entity lock na pas de RigidBody2D" << std::endl;
                            }
                            if (velocity.x > 0.f)
                            {
                                positionCible.x += cameraFollow.additionalAdvancement;
                            }
                            else if (velocity.x < 0.f)
                            {
                                positionCible.x -= cameraFollow.additionalAdvancement;
                            }
                            float shiftAdaptatifX = std::fabsf(positionCible.x - position.x)* cameraFollow.shiftSpeedMultiplicateur * cameraFollow.shiftSpeed;
                            if (velocity.y > 0.f)
                            {
                                positionCible.y += cameraFollow.additionalAdvancement;
                            }
                            else if (velocity.y < 0.f)
                            {
                                positionCible.y -= cameraFollow.additionalAdvancement;
                            }
                            float shiftAdaptatifY = std::fabsf(positionCible.y - position.y)* cameraFollow.shiftSpeedMultiplicateur * cameraFollow.shiftSpeed;
                            if (shiftAdaptatifX < cameraFollow.shiftSpeed) shiftAdaptatifX = cameraFollow.shiftSpeed;
                            if (shiftAdaptatifY < cameraFollow.shiftSpeed) shiftAdaptatifY = cameraFollow.shiftSpeed;
                            position.x = Math::moveTowards(position.x, positionCible.x, shiftAdaptatifX*dt);
                            position.y = Math::moveTowards(position.y, positionCible.y, shiftAdaptatifY*dt);
                            registry.getComponent<Transform2D>(e).setPosition(position);
                        } else {
                            std::cerr << " [Camera] l'Entity lock na pas de Transform2D" << std::endl;
                        }
                    } else {
                        std::cerr << " [Camera] est senser avoir un composant Transform2D" << std::endl;
                    }
                } else {
                    cameraFollow.targetLock = NULL_ENTITY;
                    std::cerr << " [Camera] est lock sur une Entity qui nexiste plus" << std::endl;
                }
            } else {
                std::cerr << " [Camera] Attention il y a un composant CameraFollow sans etre definie sur une entite. " << std::endl;
            }
        } else
        {
            std::cerr << " [Camera] on ne peut pas mettre le composant CameraFollow sans avoir le composant Camera2d. " << std::endl;
        }
    });
}
