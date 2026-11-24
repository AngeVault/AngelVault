//
// Created by p.pagliazzo on 27/10/2026.
//
#include "ecs/behavior.h"
#include "ecs/registry.h"
#include "components/rigid_body2d.h"
#include "input/input.h"
#include "ecs/behavior_factory.h"
#include "../components/life.h"
#include "components/geometry.h"
#include "components/sprite_animator.h"
#include "utils/camera_utils.h"
#include <cmath>
#pragma once

class PlayerBehavior : public IBehavior {
public:
    PROPERTY(float, speed, 250.f)
    PROPERTY(float, shakeOnJump, 0.4f)
    PROPERTY(std::string, idleClip, "Idle")
    PROPERTY(std::string, moveClip, "Run")

        void onUpdate(Entity self, Registry& registry, float dt) override
    {

        keyTimer += dt;

        auto& rb = registry.getComponent<RigidBody2D>(self);
        const bool alive = !registry.hasComponent<Life>(self) || registry.getComponent<Life>(self).actualLife > 0.f;
        if (alive)
        {
            // Input Manager (assets/input.cfg) : flèches, ZQSD/WASD et stick gauche par défaut  
            float moveX = Input::getAxis("Horizontal");
            float moveY = Input::getAxis("Vertical");
            const float len = std::hypot(moveX, moveY);
            if (len > 1.f) { moveX /= len; moveY /= len; }

            rb.velocity.x = moveX * speed;
            rb.velocity.y = moveY * speed;

            // garde la dernière direction à l'arrêt
            if (registry.hasComponent<SpriteAnimator>(self)) {
                auto& anim = registry.getComponent<SpriteAnimator>(self);
                const bool moving = moveX != 0.f || moveY != 0.f;
                anim.clip = moving ? moveClip : idleClip;
                if (moving) anim.facing = { moveX, moveY };
            }

            if (Input::getButtonDown("saute")) Camera::shake(registry, shakeOnJump);   // démo CameraShake
        }
    }

private:
    float keyTimer = 0;
};

// Nom utilisé dans les fichiers .scene ("behavior": "PlayerBehavior")
REGISTER_BEHAVIOR(PlayerBehavior)
