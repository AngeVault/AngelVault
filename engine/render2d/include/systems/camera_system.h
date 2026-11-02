//
// Created by p.pagliazzo on 27/10/2026.
//

#pragma once
#include "ecs/system.h"
#include "components/camera2d.h"
#include "ecs/entity.h"
#include "components/transform2d.h"
#include "ecs/registry.h"
#include "input/input.h"
#include "math/interpolation.h"
#include "math/easing.h"
#include "components/rigid_body2d.h"
#include <math.h>

#include <algorithm>
#include <cmath>
#include <SFML/Graphics.hpp>

// Entrées utilisées (actions de l'Input Manager, voir assets/input.cfg du projet) :
//   CameraX / CameraY : déplacer une caméra libre        Zoom : zoom continu (touches, gâchettes)
//   ZoomWheel         : zoom par crans (molette)
class CameraSystem : public ISystem
{
public:
    static constexpr const char* kActionPanX = "CameraX";
    static constexpr const char* kActionPanY = "CameraY";
    static constexpr const char* kActionZoom = "Zoom";
    static constexpr const char* kActionZoomWheel = "ZoomWheel";

    CameraSystem(sf::RenderWindow& window) : m_window(window) {priority = -10;} ;
    sf::View makeView(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize) const;

    void onUpdate(Registry& registry,float dt) override;;

    void clampPosition(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize);;

private:
    sf::RenderWindow& m_window;
};

