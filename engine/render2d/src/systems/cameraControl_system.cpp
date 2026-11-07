//
// Created by pagli on 07/11/2026.
//

#pragma once

#include "systems/cameraControl_system.h"
#include <iostream>
#include "components/cameraControl.h"
#include "ecs/entity.h"
#include "components/transform2d.h"
#include "components/camera2d.h"
#include "components/cameraFollow.h"
#include "input/input.h"
#include "ecs/registry.h"

void CameraControlSystem::onUpdate(Registry &registry, float dt)
{
    registry.view<CameraControl>([&](Entity e, const CameraControl& cameraControl)
    {
        if (registry.hasComponent<Camera2d>(e))
        {
            Camera2d& camera2d = registry.getComponent<Camera2d>(e);
            if (cameraControl.zoomLocked == false)
            {
                camera2d.targetZoom += (camera2d.zoomStep *dt) * Input::getAxis("ZoomWheel") ;
                if (camera2d.targetZoom < cameraControl.minZoom) camera2d.targetZoom = cameraControl.minZoom;
                if (camera2d.targetZoom > cameraControl.maxZoom) camera2d.targetZoom = cameraControl.maxZoom;

            }
            bool loked = false;
            if (registry.hasComponent<CameraFollow>(e))
            {
                if (registry.getComponent<CameraFollow>(e).targetLock != NULL_ENTITY) loked = true;
            }
            if (loked == false)
            {
                if (registry.hasComponent<Transform2D>(e))
                {
                    sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
                    position.x += (cameraControl.panSpeedKeyboard / camera2d.zoom *dt) * Input::getAxis("Horizontal");
                    position.y += (cameraControl.panSpeedKeyboard / camera2d.zoom *dt) * Input::getAxis("Vertical");
                    registry.getComponent<Transform2D>(e).setPosition(position);
                } else {
                    std::cerr << " [Camera] est senser avoir un composant Transform2D" << std::endl;
                }
            }
        } else
        {
            std::cerr << " [Camera] on ne peut pas mettre le composant CameraControl sans avoir le composant Camera2d. " << std::endl;
        }
    });

}
