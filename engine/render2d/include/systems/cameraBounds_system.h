//
// Created by pagli on 07/11/2026.
//

#pragma once
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include "ecs/entity.h"

class CameraBoundsSystem : public ISystem
{
public:

    CameraBoundsSystem(sf::RenderWindow& window) : m_window(window) {priority = -10;} ;
    void onUpdate(Registry& registry,float dt) override;;

private:
    sf::RenderWindow& m_window;
};
