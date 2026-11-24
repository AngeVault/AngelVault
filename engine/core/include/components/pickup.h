#pragma once
#include "ecs/reflect.h"
#include <string>

// objet au sol : va dans l'Inventory qui le touche, ou qui interagit avec s'il est Interactable
struct Pickup {
    std::string item;
    int count = 1;
};

template<>
inline ReflectedComponent reflectComponent<Pickup>(Pickup& p) {
    return { "Pickup", {
        { "item", PropertyInfo::Type::String, &p.item },
        { "count", PropertyInfo::Type::Int, &p.count },
    } };
}
