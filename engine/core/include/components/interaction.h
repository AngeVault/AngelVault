#pragma once
#include "ecs/reflect.h"
#include <string>

// déclenche les Interactable qu'il touche avec son action d'entrée
struct Interactor {
    std::string action = "interagir";
};

// réagit seulement à l'action d'un Interactor, plus au simple contact
struct Interactable {
    bool enabled = true;   // false : verrouillé
};

template<>
inline ReflectedComponent reflectComponent<Interactor>(Interactor& i) {
    return { "Interactor", { { "action", PropertyInfo::Type::String, &i.action } } };
}

template<>
inline ReflectedComponent reflectComponent<Interactable>(Interactable& i) {
    return { "Interactable", { { "enabled", PropertyInfo::Type::Bool, &i.enabled } } };
}
