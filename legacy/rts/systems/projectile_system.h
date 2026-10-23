#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/projectileComp.h"
#include <SFML/System/Vector2.hpp>

class ProjectileSystem : public ISystem {
public:
    ProjectileSystem() { priority = 0; }

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Projectile, Transform2D>([&](Entity e, Projectile& p, Transform2D& t) {
            if (p.hasHit) return;
            p.life -= dt;
            if (p.life <= 0.f) {
                p.hasHit = true;
                registry.destroyEntity(e);
                return;
            }
            t.move(p.dir * p.speed * dt);
            });
    }
};
