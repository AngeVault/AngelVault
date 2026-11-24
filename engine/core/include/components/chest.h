#pragma once
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>
#include <string>

// s'ouvre quand on interagit avec, son contenu est l'Inventory de la même entité
struct Chest {
    std::string key; // objet à avoir et consommé à l'ouverture, vide : pas verrouillé
    bool opened = false;
    std::string openTexture; // texture une fois ouvert, vide : la même
    sf::Vector2f openOffset; // décalage du rectangle du sprite une fois ouvert, pour une planche
    float openTime = 0.45f; // durée de l'animation d'ouverture, 0 : instantané
};

template<>
inline ReflectedComponent reflectComponent<Chest>(Chest& c) {
    return { "Chest", {
        { "key", PropertyInfo::Type::String, &c.key },
        { "opened", PropertyInfo::Type::Bool, &c.opened },
        { "openTexture", PropertyInfo::Type::String, &c.openTexture },
        { "openOffset", PropertyInfo::Type::Vec2f, &c.openOffset },
        { "openTime", PropertyInfo::Type::Float, &c.openTime },
    } };
}
