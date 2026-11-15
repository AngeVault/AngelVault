#pragma once
// change de map quand le joueur touche un MapTransition
#include "game_loop.h"
#include "scene/scene_transition.h"
#include "components/dont_destroy.h"
#include "components/map_transition.h"
#include "systems/collision_system.h"

#include <iostream>
#include <optional>
#include <string>

class SceneTransitionSystem : public ISystem {
public:
    SceneTransitionSystem(Game& game, CollisionSystem& collision) : m_game(&game), m_collision(&collision) {
        priority = 1;   // juste après CollisionSystem
        collision.onCollision = [this](const CollisionInfo& info) { onTrigger(m_game->registry(), info); };
    }

    void onTrigger(Registry& registry, const CollisionInfo& info) {
        if (m_pending) return;   // un changement est déjà en route
        tryTrigger(registry, info.entityA, info.entityB);
        tryTrigger(registry, info.entityB, info.entityA);
    }

    void onUpdate(Registry& registry, float) override {
        if (!m_pending) return;
        const Pending req = *m_pending;
        m_pending.reset();
        const SceneChangeResult r = changeScene(registry, m_game->assetDir(), req.scene, req.spawn);
        m_collision->forgetPositions();
        for (const std::string& w : r.warnings) std::cerr << "[Scene] " << w << "\n";
        if (!r.ok) std::cerr << "[Scene] " << r.error << "\n";
    }

private:
    struct Pending { std::string scene, spawn; };
    Game* m_game;
    CollisionSystem* m_collision;
    std::optional<Pending> m_pending;

    void tryTrigger(Registry& registry, Entity door, Entity other) {
        if (!registry.isValid(door) || !registry.hasComponent<MapTransition>(door)) return;
        if (!registry.isValid(other) || !registry.hasComponent<DontDestroy>(other)) return;
        if (registry.getComponent<DontDestroy>(other).key != "Player") return;
        const MapTransition& mt = registry.getComponent<MapTransition>(door);
        m_pending = Pending{ mt.scene, mt.spawn };
    }
};
