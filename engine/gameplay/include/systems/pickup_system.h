#pragma once
#include "components/interaction.h"
#include "components/inventory.h"
#include "components/persistent_id.h"
#include "components/pickup.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "systems/collision_system.h"
#include "systems/interaction_system.h"

#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// met les Pickup dans l'Inventory qui les touche (ou qui interagit avec s'ils sont Interactable)
class PickupSystem : public ISystem {
public:
    using Listener = std::function<void(Registry& registry, Entity collector, const std::string& item, int count)>;

    PickupSystem(CollisionSystem& collision, InteractionSystem& interaction) : m_collision(&collision) {
        priority = 1;   // lit les contacts de CollisionSystem, à ajouter après lui
        interaction.addListener([this](Registry& registry, Entity who, Entity target) { collect(registry, who, target); });
    }

    void addListener(Listener listener) { m_listeners.push_back(std::move(listener)); }

    void onUpdate(Registry& registry, float) override {
        // déjà ramassé pendant la partie : ne revient pas quand on recharge la map
        registry.view<Pickup, PersistentId>([&](Entity e, Pickup&, PersistentId& id) {
            if (m_collected.count(id.value)) registry.destroyEntity(e);
        });
        // on collecte d'abord : ramasser détruit des entités
        std::vector<std::pair<Entity, Entity>> touches;
        for (const CollisionInfo& c : m_collision->contacts()) {
            touches.push_back({ c.entityA, c.entityB });
            touches.push_back({ c.entityB, c.entityA });
        }
        for (const auto& [who, item] : touches)
            if (registry.isValid(item) && registry.hasComponent<Pickup>(item) && !registry.hasComponent<Interactable>(item))
                collect(registry, who, item);
    }

    bool collected(uint64_t id) const { return m_collected.count(id) > 0; }

private:
    CollisionSystem* m_collision;
    std::vector<Listener> m_listeners;
    std::unordered_set<uint64_t> m_collected;

    void collect(Registry& registry, Entity who, Entity target) {
        if (!registry.isValid(who) || !registry.isValid(target)) return;
        if (!registry.hasComponent<Inventory>(who) || !registry.hasComponent<Pickup>(target)) return;
        Pickup& p = registry.getComponent<Pickup>(target);
        if (p.count <= 0) return;   // déjà pris cette image, destruction en attente
        const int left = registry.getComponent<Inventory>(who).add(p.item, p.count);
        const int taken = p.count - left;
        if (taken == 0) return;   // inventaire plein
        p.count = left;
        if (left == 0) {
            if (registry.hasComponent<PersistentId>(target)) m_collected.insert(registry.getComponent<PersistentId>(target).value);
            registry.destroyEntity(target);
        }
        for (const Listener& l : m_listeners) l(registry, who, p.item, taken);
    }
};
