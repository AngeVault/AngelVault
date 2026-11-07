//
// Created by pagli on 07/11/2026.
//
#pragma once

#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

struct CameraBounds
{
    // border
    sf::Vector2f worldMin{ 0.f , 0.f };
    sf::Vector2f worldMax{ 0.f , 0.f };

    // helper
    void setWorldBorder(sf::Vector2f min, sf::Vector2f max)
    {
        worldMin = min;
        worldMax = max;
    }

};
template<>
inline ReflectedComponent reflectComponent<CameraBounds>(CameraBounds& rb) {
    return { "CameraBounds", {
	            { "worldMin",       PropertyInfo::Type::Vec2f, &rb.worldMin       },
                { "worldMax", PropertyInfo::Type::Vec2f, &rb.worldMax },
        } };
}