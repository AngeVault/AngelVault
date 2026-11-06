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

    // Écran de référence (0 x 0 = la taille de la fenêtre, le cas de rpg.exe).
    // L'éditeur met 1920 x 1080 : en jeu, la caméra montre la même zone que son cadre dans la vue scène,
    // quelle que soit la taille du panneau (bandes sur les côtés si le format n'est pas le même).
    static void setReferenceSize(sf::Vector2u size) { s_referenceSize = size; }
    static sf::Vector2u referenceSize() { return s_referenceSize; }
    // Zone de la fenêtre (en 0..1) qui garde le format de l'écran de référence, centrée
    static sf::FloatRect letterbox(sf::Vector2u window, sf::Vector2u reference);
    sf::View makeView(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize) const;

    void onUpdate(Registry& registry,float dt) override;;

    void clampPosition(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize);;

private:
    sf::RenderWindow& m_window;
    inline static sf::Vector2u s_referenceSize{ 0u, 0u };
};

