#pragma once
#include "components/chest.h"
#include "components/inventory.h"
#include "components/quest.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "systems/interaction_system.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// suit l'état des quêtes : un QuestGiver les donne, un QuestObjective les accomplit
class QuestSystem : public ISystem {
public:
    enum class State { None, Active, Done };
    using Listener = std::function<void(Registry& registry, const std::string& quest, State state)>;

    // à ajouter après ChestSystem : un coffre doit être ouvert avant qu'on lise son état
    explicit QuestSystem(InteractionSystem& interaction) {
        priority = 1;
        interaction.addListener([this](Registry& registry, Entity who, Entity target) { use(registry, who, target); });
    }

    void addListener(Listener listener) { m_listeners.push_back(std::move(listener)); }

    void onUpdate(Registry&, float) override {}

    State state(const std::string& quest) const {
        const auto it = m_states.find(quest);
        return it == m_states.end() ? State::None : it->second;
    }

    void setState(Registry& registry, const std::string& quest, State s) {
        if (quest.empty() || state(quest) == s) return;
        m_states[quest] = s;
        std::cout << "[Quete] " << quest << (s == State::Active ? " acceptee" : s == State::Done ? " accomplie" : " oubliee") << std::endl;
        for (const Listener& l : m_listeners) l(registry, quest, s);
    }

private:
    std::vector<Listener> m_listeners;
    std::unordered_map<std::string, State> m_states;   // garde l'avancée d'une map à l'autre

    void use(Registry& registry, Entity who, Entity target) {
        if (!registry.isValid(target)) return;
        if (registry.hasComponent<QuestGiver>(target)) {
            const std::string& quest = registry.getComponent<QuestGiver>(target).quest;
            if (state(quest) == State::None) setState(registry, quest, State::Active);
        }
        if (!registry.hasComponent<QuestObjective>(target)) return;
        if (registry.hasComponent<Chest>(target) && !registry.getComponent<Chest>(target).opened) return;   // coffre verrouillé
        const QuestObjective o = registry.getComponent<QuestObjective>(target);
        if (state(o.quest) != State::Active) return;
        if (!o.reward.empty() && o.rewardCount > 0 && registry.hasComponent<Inventory>(who)) registry.getComponent<Inventory>(who).add(o.reward, o.rewardCount);
        setState(registry, o.quest, State::Done);
    }
};
