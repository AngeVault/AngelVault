//
// Created by p.pagliazzo on 10/11/2026.
//

#pragma once
#include <SFML/System/Vector2.hpp>
#include "ecs/reflect.h"
#include "ecs/property.h"

struct SteeringSeekComponent
{
    bool isActif = true;
    sf::Vector2f positionCible; 
};
template<>
inline ReflectedComponent reflectComponent<SteeringSeekComponent>(SteeringSeekComponent& rb) {
    return { "SteeringSeekComponent", {
                { "isActif",       PropertyInfo::Type::Bool, &rb.isActif       },
                { "positionCible", PropertyInfo::Type::Vec2f, &rb.positionCible }
    } };
}