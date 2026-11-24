#pragma once
#include "ecs/reflect.h"
#include <string>

// donne sa quête à l'Interactor qui interagit avec
struct QuestGiver {
    std::string quest;
};

// accomplit la quête si elle est en cours quand on interagit avec, la récompense va dans l'Inventory de l'Interactor
struct QuestObjective {
    std::string quest;
    std::string reward;
    int rewardCount = 1;
};

template<>
inline ReflectedComponent reflectComponent<QuestGiver>(QuestGiver& q) {
    return { "QuestGiver", { { "quest", PropertyInfo::Type::String, &q.quest } } };
}

template<>
inline ReflectedComponent reflectComponent<QuestObjective>(QuestObjective& q) {
    return { "QuestObjective", {
        { "quest", PropertyInfo::Type::String, &q.quest },
        { "reward", PropertyInfo::Type::String, &q.reward },
        { "rewardCount", PropertyInfo::Type::Int, &q.rewardCount },
    } };
}
