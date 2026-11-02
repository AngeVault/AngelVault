//
// Created by p.pagliazzo on 27/10/2026.
//
#include "ecs/behavior.h"
#include "ecs/registry.h"
#include "components/rigid_body2d.h"
#include "input/input.h"
#include "ecs/behavior_factory.h"
#include "components/geometry.h"
#pragma once

class PlayerBehavior : public IBehavior {
public:
    PROPERTY(float, speed, 250.f)

        void onUpdate(Entity self, Registry& registry, float dt) override
        {

        keyTimer += dt;

        auto& rb = registry.getComponent<RigidBody2D>(self);

        // Actions de l'Input Manager (assets/input.cfg) : flèches, ZQSD/WASD et stick gauche par défaut
        const float moveX = Input::getAxis("Horizontal");
        const float moveY = Input::getAxis("Vertical");

        rb.velocity.x = moveX * speed;
        rb.velocity.y = moveY * speed;
    }

private:
    float keyTimer = 0;
};

// Nom utilisé dans les fichiers .scene ("behavior": "PlayerBehavior")
REGISTER_BEHAVIOR(PlayerBehavior)
