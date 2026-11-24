#pragma once
#include "components/interaction.h"
#include "components/inventory.h"
#include "components/name.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "input/input.h"

#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

// debug console : inventaire des Interactor sur une action, et chaque objet gagné ou perdu
class InventoryLogSystem : public ISystem {
public:
    explicit InventoryLogSystem(std::string action = "inventaire") : m_action(std::move(action)) {
        priority = 3;   // après le ramassage et les coffres
    }

    void onUpdate(Registry& registry, float) override {
        const bool show = Input::getButtonDown(m_action);
        registry.view<Interactor, Inventory>([&](Entity e, Interactor&, Inventory& inv) {
            const std::string name = registry.hasComponent<NameComponent>(e) ? registry.getComponent<NameComponent>(e).name : "?";
            logChanges(key(e), name, inv);
            if (show) print(name, inv);
        });
    }

private:
    std::string m_action;
    std::unordered_map<uint64_t, std::map<std::string, int>> m_last;   // quantités vues à l'image d'avant

    static uint64_t key(Entity e) { return (uint64_t(e.index) << 32) | e.generation; }

    static std::map<std::string, int> totals(const Inventory& inv) {
        std::map<std::string, int> t;
        for (const ItemStack& s : inv.stacks) t[s.item] += s.count;
        return t;
    }

    void logChanges(uint64_t id, const std::string& name, const Inventory& inv) {
        std::map<std::string, int> now = totals(inv);
        const auto it = m_last.find(id);
        if (it != m_last.end()) {   // première image : contenu de départ, pas un gain
            std::map<std::string, int> diff = now;
            for (const auto& [item, n] : it->second) diff[item] -= n;
            for (const auto& [item, n] : diff)
                if (n != 0) std::cout << "[Inventaire] " << name << " " << (n > 0 ? "+" : "") << n << " " << item << std::endl;
        }
        m_last[id] = std::move(now);
    }

    static void print(const std::string& name, const Inventory& inv) {
        std::cout << "[Inventaire] " << name << " (" << inv.stacks.size();
        if (inv.capacity > 0) std::cout << "/" << inv.capacity;
        std::cout << " cases)" << std::endl;
        if (inv.stacks.empty()) std::cout << "  vide" << std::endl;
        for (const ItemStack& s : inv.stacks) std::cout << "  " << s.item << " x" << s.count << std::endl;
    }
};
