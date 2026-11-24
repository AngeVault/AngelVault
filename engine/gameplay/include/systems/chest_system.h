#pragma once
#include "components/chest.h"
#include "components/geometry.h"
#include "components/interaction.h"
#include "components/inventory.h"
#include "components/persistent_id.h"
#include "components/transform2d.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "math/easing.h"
#include "systems/interaction_system.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

// ouvre un Chest quand on interagit avec et vide son Inventory dans celui de l'Interactor
class ChestSystem : public ISystem {
public:
    using Listener = std::function<void(Registry& registry, Entity opener, Entity chest)>;

    explicit ChestSystem(InteractionSystem& interaction) {
        priority = 1;
        interaction.addListener([this](Registry& registry, Entity who, Entity target) { use(registry, who, target); });
    }

    // appelé à la première ouverture seulement
    void addListener(Listener listener) { m_listeners.push_back(std::move(listener)); }

    void onUpdate(Registry& registry, float dt) override {
        // déjà ouvert pendant la partie : retrouve son état quand on recharge la map
        registry.view<Chest, PersistentId>([&](Entity e, Chest& c, PersistentId& id) {
            if (c.opened) return;
            const auto it = m_left.find(id.value);
            if (it == m_left.end()) return;
            c.opened = true;
            showOpen(registry, e, c);
            if (registry.hasComponent<Inventory>(e)) registry.getComponent<Inventory>(e).stacks = it->second;
            disableIfEmpty(registry, e);
        });
        animate(registry, dt);
    }

private:
    std::vector<Listener> m_listeners;
    std::unordered_map<uint64_t, std::vector<ItemStack>> m_left;   // ce qui reste dans chaque coffre ouvert

    struct Opening { Entity chest; float time = 0.f; sf::Vector2f scale; bool shown = false; };
    std::vector<Opening> m_openings;
    static constexpr float kSquash = 0.25f;   // part de l'animation où le coffre s'écrase avant de s'ouvrir

    void use(Registry& registry, Entity who, Entity target) {
        if (!registry.isValid(target) || !registry.hasComponent<Chest>(target)) return;
        Chest& c = registry.getComponent<Chest>(target);
        if (!c.opened) {
            if (!c.key.empty()) {
                if (!registry.hasComponent<Inventory>(who) || !registry.getComponent<Inventory>(who).has(c.key)) return;   // verrouillé
                registry.getComponent<Inventory>(who).remove(c.key, 1);
            }
            c.opened = true;
            if (c.openTime > 0.f && registry.hasComponent<Transform2D>(target))
                m_openings.push_back({ target, 0.f, registry.getComponent<Transform2D>(target).getScale() });
            else
                showOpen(registry, target, c);
            for (const Listener& l : m_listeners) l(registry, who, target);
        }
        if (registry.hasComponent<Inventory>(target) && registry.hasComponent<Inventory>(who)) {
            Inventory& from = registry.getComponent<Inventory>(target);
            Inventory& to = registry.getComponent<Inventory>(who);
            for (ItemStack& s : from.stacks) s.count = to.add(s.item, s.count);
            std::erase_if(from.stacks, [](const ItemStack& s) { return s.count <= 0; });
        }
        if (registry.hasComponent<PersistentId>(target)) {
            const auto& stacks = registry.hasComponent<Inventory>(target) ? registry.getComponent<Inventory>(target).stacks : std::vector<ItemStack>{};
            m_left[registry.getComponent<PersistentId>(target).value] = stacks;
        }
        disableIfEmpty(registry, target);
    }

    // s'écrase, passe au sprite ouvert puis rebondit jusqu'à sa taille
    void animate(Registry& registry, float dt) {
        for (Opening& o : m_openings) {
            if (!registry.isValid(o.chest) || !registry.hasComponent<Chest>(o.chest) || !registry.hasComponent<Transform2D>(o.chest)) { o.time = -1.f; continue; }
            const Chest& c = registry.getComponent<Chest>(o.chest);
            o.time += dt;
            const float t = c.openTime > 0.f ? std::min(o.time / c.openTime, 1.f) : 1.f;
            sf::Vector2f k;
            if (t < kSquash) {
                const float e = Ease::outQuad(t / kSquash);
                k = { 1.f + 0.12f * e, 1.f - 0.18f * e };
            } else {
                if (!o.shown) { showOpen(registry, o.chest, c); o.shown = true; }
                const float e = Ease::outElastic((t - kSquash) / (1.f - kSquash));
                k = { 1.12f + (1.f - 1.12f) * e, 0.82f + (1.f - 0.82f) * e };
            }
            if (t >= 1.f) { k = { 1.f, 1.f }; o.time = -1.f; }
            registry.getComponent<Transform2D>(o.chest).setScale({ o.scale.x * k.x, o.scale.y * k.y });
        }
        std::erase_if(m_openings, [](const Opening& o) { return o.time < 0.f; });
    }

    static void showOpen(Registry& registry, Entity e, const Chest& c) {
        if (!registry.hasComponent<Geometry>(e)) return;
        auto* sprite = std::get_if<Geometry::SpriteData>(&registry.getComponent<Geometry>(e).data);
        if (!sprite) return;
        if (!c.openTexture.empty()) sprite->textureId = c.openTexture;
        sprite->rect.position += sf::Vector2i(c.openOffset);
    }

    // vide : on ne le vise plus, l'action va à ce qui est à côté
    static void disableIfEmpty(Registry& registry, Entity e) {
        if (!registry.hasComponent<Interactable>(e)) return;
        const bool empty = !registry.hasComponent<Inventory>(e) || registry.getComponent<Inventory>(e).stacks.empty();
        if (empty) registry.getComponent<Interactable>(e).enabled = false;
    }
};
