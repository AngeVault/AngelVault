//
// Created by pagli on 07/11/2026.
//

#pragma once
#include "systems/cameraBounds_system.h"
#include "components/cameraBounds.h"
#include "components/cameraControl.h"
#include "components/camera2d.h"
#include "components/transform2d.h"
#include <iostream>
#include "ecs/registry.h"

void CameraBoundsSystem::onUpdate(Registry &registry, float dt) {
    registry.view<CameraBounds>([&](Entity e, const CameraBounds& cameraBounds) {
        if (registry.hasComponent<Camera2d>(e)) {
            Camera2d& camera2d = registry.getComponent<Camera2d>(e);
            const sf::Vector2u viewportSize = m_window.getSize();
            const float worldWidth = cameraBounds.worldMax.x - cameraBounds.worldMin.x;
            const float worldHeight = cameraBounds.worldMax.y - cameraBounds.worldMin.y;
            if (registry.hasComponent<Transform2D>(e))
            {
                if (registry.hasComponent<CameraControl>(e))
                {
                    CameraControl& cameraControl = registry.getComponent<CameraControl>(e);
                    if (viewportSize.x > 0 && viewportSize.y > 0 && worldWidth > 0.f && worldHeight > 0.f)
                    {
                        // Empêche le zoom arrière d'élargir la vue au-delà de la carte.
                        const float fitZoom = std::max(
                            static_cast<float>(viewportSize.x) / worldWidth,
                            static_cast<float>(viewportSize.y) / worldHeight);
                        cameraControl.minZoom = std::min(fitZoom, cameraControl.maxZoom);
                        camera2d.zoom = std::max(camera2d.zoom, cameraControl.minZoom);
                        camera2d.targetZoom = std::clamp(camera2d.targetZoom,
                        cameraControl.minZoom, cameraControl.maxZoom);
                    }
                }

                sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
                if (cameraBounds.worldMax.x > cameraBounds.worldMin.x && cameraBounds.worldMax.y > cameraBounds.worldMin.y)
                {
                    float halfW = (viewportSize.x * 0.5f) /camera2d.zoom;
                    float halfH = (viewportSize.y * 0.5f) /camera2d.zoom;

                    float minX = cameraBounds.worldMin.x + halfW;
                    float maxX = cameraBounds.worldMax.x - halfW;
                    float minY = cameraBounds.worldMin.y + halfH;
                    float maxY = cameraBounds.worldMax.y - halfH;

                    if (minX > maxX) position.x = (cameraBounds.worldMin.x + cameraBounds.worldMax.x) * 0.5f;
                    else position.x = std::clamp(position.x, minX, maxX);
                    if (minY > maxY) position.y = (cameraBounds.worldMin.y + cameraBounds.worldMax.y) * 0.5f;
                    else position.y = std::clamp(position.y, minY, maxY);
                    registry.getComponent<Transform2D>(e).setPosition(position);
                } else {
                    std::cerr << " [Camera] le composant CameraBounds ne peut pas avoir un min superieur ou egal au max" << std::endl;
                }
            } else {
                std::cerr << " [Camera] est senser avoir un composant Transform2D" << std::endl;
            }
        } else {
            std::cerr << " [Camera] on ne peut pas mettre le composant cameraBounds sans avoir le composant Camera2d. " << std::endl;
        }
    });
}
