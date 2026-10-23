#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "components/rtsPlayerState.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <climits>
#include <cmath>

class HealSystem : public ISystem {
public:
    HealSystem() { priority = -4; } // avant le combat

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) return;

        // cooldowns des soigneurs
        registry.view<ECS::Unit>([&](Entity, ECS::Unit& u) {
            if (u.healTimer > 0.f) u.healTimer -= dt;
            });

        registry.view<ECS::Unit, Transform2D>([&](Entity self, ECS::Unit& u, Transform2D& t) {
            if (u.type != ECS::Unit::Type::Healer) return;
            if (u.pv <= 0 || u.healTimer > 0.f) return;

            // cherche l'allié le plus blessé à portée
            Entity ally = NULL_ENTITY;
            int lowestPv = INT_MAX;
            registry.view<ECS::Unit, Transform2D>([&](Entity e, ECS::Unit& a, Transform2D& at) {
                if (e.index == self.index) return;   // ne se soigne pas lui-même
                if (a.team != u.team) return;        // allié uniquement
                if (a.pv >= a.pvMax) return;         // déjà plein
                float dx = at.getPosition().x - t.getPosition().x;
                float dy = at.getPosition().y - t.getPosition().y;
                float d = std::hypot(dx, dy);
                if (d <= u.healRange && a.pv < lowestPv) {
                    lowestPv = a.pv;
                    ally = e;
                }
                });

            if (ally.index == UINT32_MAX) return;

            ECS::Unit& healed = registry.getComponent<ECS::Unit>(ally);
            healed.pv = std::min(healed.pvMax, healed.pv + u.healAmount);
            u.healTimer = u.healCooldown;
            });
    }
};
