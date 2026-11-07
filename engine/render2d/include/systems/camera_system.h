//
// Created by p.pagliazzo on 27/10/2026.
//

#pragma once
#include "ecs/system.h"
#include "components/camera2d.h"
#include "ecs/entity.h"
#include "components/transform2d.h"


#include <SFML/Graphics.hpp>

// Entrées utilisées (actions de l'Input Manager, voir assets/input.cfg du projet) :
//   CameraX / CameraY : déplacer une caméra libre        Zoom : zoom continu (touches, gâchettes)
//   ZoomWheel         : zoom par crans (molette)
class CameraSystem : public ISystem
{
public:

    CameraSystem(sf::RenderWindow& window) : m_window(window) {priority = -10;} ;
    sf::View makeView(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize) const;

    void onUpdate(Registry& registry,float dt) override;;

private:
    sf::RenderWindow& m_window;
};

