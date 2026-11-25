#pragma once
#include "components/interaction.h"
#include "components/script_component.h"
#include "components/transform2d.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "input/input.h"
#include "systems/collision_system.h"

#include <functional>
#include <utility>
#include <vector>

// un Interactor qui appuie sur son action déclenche l'Interactable le plus proche qu'il touche
class InteractionSystem : public ISystem {
public:
    using Listener = std::function<void(Registry& registry, Entity interactor, Entity target)>;

    explicit InteractionSystem(CollisionSystem& collision) : m_collision(&collision) {
        priority = 1;   // lit les contacts de CollisionSystem, à ajouter après lui
    }

    void addListener(Listener listener) { m_listeners.push_back(std::move(listener)); }

    void onUpdate(Registry& registry, float) override {
        // on collecte d'abord : une réaction peut créer ou détruire des entités
        std::vector<std::pair<Entity, Entity>> hits;
        registry.view<Interactor, Transform2D>([&](Entity self, Interactor& in, Transform2D& t) {
            if (!Input::getButtonDown(in.action)) return;
            const Entity target = nearest(registry, self, t.getPosition());
            if (!(target == NULL_ENTITY)) hits.push_back({ self, target });
        });
        for (const auto& [who, target] : hits) interact(registry, who, target);
    }

private:
    CollisionSystem* m_collision;
    std::vector<Listener> m_listeners;

    Entity nearest(Registry& registry, Entity self, sf::Vector2f from) const {
        Entity best = NULL_ENTITY;
        float bestDist = 0.f;
        for (const CollisionInfo& c : m_collision->contacts()) {
            const Entity other = c.entityA == self ? c.entityB : c.entityB == self ? c.entityA : NULL_ENTITY;
            if (other == NULL_ENTITY || !registry.isValid(other) || !registry.hasComponent<Interactable>(other)) continue;
            if (!registry.getComponent<Interactable>(other).enabled) continue;
            sf::Vector2f d{};
            if (registry.hasComponent<Transform2D>(other)) d = registry.getComponent<Transform2D>(other).getPosition() - from;
            const float dist = d.x * d.x + d.y * d.y;
            if (best == NULL_ENTITY || dist < bestDist) { best = other; bestDist = dist; }
        }
        return best;
    }

    void interact(Registry& registry, Entity who, Entity target) {
        for (const Listener& l : m_listeners) l(registry, who, target);
        if (!registry.isValid(target) || !registry.hasComponent<Script>(target)) return;
        Script& s = registry.getComponent<Script>(target);
        for (std::size_t i = 0; i < s.behaviors.size(); ++i)
            if (IBehavior* b = s.behaviors[i].behavior.get()) b->onInteract(target, registry, who);
    }
};
