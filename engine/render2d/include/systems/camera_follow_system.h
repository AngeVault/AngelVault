#pragma once
// Déplace les entités CameraFollow vers leur cible : zone morte, avance (look-ahead), ressort amorti.
// Après la physique et les scripts : la cible est à sa place de la frame.
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/camera_follow.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"
#include "math/interpolation.h"

#include <cmath>
#include <limits>

class CameraFollowSystem : public ISystem {
public:
    CameraFollowSystem() { priority = 4; }

    void onUpdate(Registry& registry, float dt) override {
        registry.view<CameraFollow, Transform2D>([&](Entity e, CameraFollow& f, Transform2D& t) {
            if (f.target == NULL_ENTITY || f.target == e) return;
            if (!registry.isValid(f.target)) { f.target = NULL_ENTITY; return; }   // cible détruite : libre
            if (!registry.hasComponent<Transform2D>(f.target)) return;

            // Avance dans le sens du mouvement, lissée (sinon la caméra saute à chaque demi-tour)
            sf::Vector2f wanted{ 0.f, 0.f };
            if (f.lookAhead > 0.f && registry.hasComponent<RigidBody2D>(f.target)) {
                const sf::Vector2f v = registry.getComponent<RigidBody2D>(f.target).velocity;
                if (Math::length(v) > 1.f) wanted = Math::normalize(v) * f.lookAhead;
            }
            f.lookAheadOffset = f.lookAheadTime > 0.f
                ? Math::smoothDamp(f.lookAheadOffset, wanted, f.lookAheadVelocity, f.lookAheadTime, dt)
                : wanted;

            sf::Vector2f goal = registry.getComponent<Transform2D>(f.target).getPosition() + f.offset + f.lookAheadOffset;
            if (!f.started) {
                f.started = true;
                if (f.snapOnStart) { t.setPosition(goal); f.velocity = {}; return; }
            }

            // Zone morte : on ne rattrape que ce qui en sort
            const sf::Vector2f pos = t.getPosition();
            goal.x = deadZone(pos.x, goal.x, f.deadZone.x);
            goal.y = deadZone(pos.y, goal.y, f.deadZone.y);

            const float maxSpeed = f.maxSpeed > 0.f ? f.maxSpeed : std::numeric_limits<float>::infinity();
            if (f.smoothTime > 0.f) t.setPosition(Math::smoothDamp(pos, goal, f.velocity, f.smoothTime, dt, maxSpeed));
            else if (f.maxSpeed > 0.f) t.setPosition(Math::moveTowards(pos, goal, maxSpeed * dt));
            else t.setPosition(goal);
        });
    }

private:
    static float deadZone(float pos, float goal, float half) {
        if (half <= 0.f) return goal;
        const float d = goal - pos;
        if (std::abs(d) <= half) return pos;
        return goal - (d > 0.f ? half : -half);
    }
};
