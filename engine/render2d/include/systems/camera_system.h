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
    sf::View makeView(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize) const {
        sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
        sf::Vector2f half =
        {
            windowSize.x * 0.5f / camera.zoom,
            windowSize.y * 0.5f / camera.zoom
        };
        return sf::View(sf::FloatRect(
            { position.x - half.x, position.y - half.y },
            { half.x * 2.f, half.y * 2.f }
        ));
    };
    void onUpdate(Registry& registry,float dt) override {
        registry.view<Camera2d>([&](Entity e, Camera2d& camera)
        {
            const sf::Vector2u viewportSize = m_window.getSize();
            const float worldWidth = camera.worldMax.x - camera.worldMin.x;
            const float worldHeight = camera.worldMax.y - camera.worldMin.y;
            sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
            if (camera.targetLock == NULL_ENTITY)
            {
                // Déplacement : vitesse en pixels écran / s, donc plus lent dans le monde quand on zoome
                const float speed = camera.panSpeedKeyboard / std::max(camera.zoom, 0.001f) * dt;
                position.x += Input::getAxis(kActionPanX) * speed;
                position.y += Input::getAxis(kActionPanY) * speed;
            }
            else
            {
                if (registry.isValid(camera.targetLock))
                {
                    position = registry.getComponent<Transform2D>(camera.targetLock).getPosition();
                }
                else
                {
                    camera.targetLock = NULL_ENTITY;
                }

            }
            // Zoom (caméra libre ou qui suit) : on règle la cible, le zoom la rejoint en douceur
            const float zoomHeld = Input::getAxis(kActionZoom);        // maintenu : par seconde
            const float zoomNotches = Input::getAxis(kActionZoomWheel); // crans de molette de la frame
            if (zoomHeld != 0.f) camera.targetZoom *= std::pow(1.f + camera.zoomStep, zoomHeld * dt);
            if (zoomNotches != 0.f) camera.targetZoom *= std::pow(1.f + camera.zoomStep * 0.25f, zoomNotches);
            camera.targetZoom = std::clamp(camera.targetZoom, camera.minZoom, camera.maxZoom);
            camera.zoom = Math::damp(camera.zoom, camera.targetZoom, camera.zoomLerpSpeed, dt);   // même douceur à tout FPS

            if (position != registry.getComponent<Transform2D>(e).getPosition())
            {
                registry.getComponent<Transform2D>(e).setPosition(position);
            }
            if (viewportSize.x > 0 && viewportSize.y > 0 && worldWidth > 0.f && worldHeight > 0.f) {
                    // Empêche le zoom arrière d'élargir la vue au-delà de la carte.
                    const float fitZoom = std::max(
                        static_cast<float>(viewportSize.x) / worldWidth,
                        static_cast<float>(viewportSize.y) / worldHeight);
                    camera.minZoom = std::min(fitZoom, camera.maxZoom);
                    camera.zoom = std::max(camera.zoom, camera.minZoom);
                    camera.targetZoom = std::clamp(camera.targetZoom,
                    camera.minZoom, camera.maxZoom);
                }
                clampPosition(e,registry,camera,viewportSize);
                m_window.setView(makeView(e,registry,camera,viewportSize));
        });

    };

    void clampPosition(Entity e,Registry& registry, Camera2d& camera,sf::Vector2u windowSize)
    {
        if (camera.worldMax.x <= camera.worldMin.x || camera.worldMax.y <= camera.worldMin.y) return;

        float halfW = (windowSize.x * 0.5f) /camera.zoom;
        float halfH = (windowSize.y * 0.5f) /camera.zoom;

        float minX = camera.worldMin.x + halfW;
        float maxX = camera.worldMax.x - halfW;
        float minY = camera.worldMin.y + halfH;
        float maxY = camera.worldMax.y - halfH;

        sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();

        if (minX > maxX) position.x = (camera.worldMin.x + camera.worldMax.x) * 0.5f;
        else position.x = std::clamp(position.x, minX, maxX);
        if (minY > maxY) position.y = (camera.worldMin.y + camera.worldMax.y) * 0.5f;
        else position.y = std::clamp(position.y, minY, maxY);
        registry.getComponent<Transform2D>(e).setPosition(position);
    };

private:
    sf::RenderWindow& m_window;
};

