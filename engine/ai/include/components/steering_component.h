//
// Created by p.pagliazzo on 09/11/2026.
//

#pragma once
#include <SFML/System/Vector2.hpp>
#include "ecs/reflect.h"

struct SteeringComponent
{
    sf::Vector2f direction = {0,0}; // entre -1 et 1 le total doit faire 1 ou 0;
    float rapidite = 0;
};
template<>
inline ReflectedComponent reflectComponent<SteeringComponent>(SteeringComponent& rb) {
    return { "SteeringComponent", {
            { "mass",       PropertyInfo::Type::Vec2f, &rb.direction       },
            { "linearDrag", PropertyInfo::Type::Float, &rb.rapidite }
    } };
}