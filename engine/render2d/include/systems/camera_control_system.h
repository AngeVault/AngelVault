#pragma once
// Pan / zoom de la caméra affichée (CameraControl). Avant la physique : ne dépend que des entrées.
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/camera2d.h"
#include "components/camera_control.h"
#include "components/camera_follow.h"
#include "components/transform2d.h"
#include "input/input.h"
#include "math/interpolation.h"
#include "systems/camera_system.h"

#include <algorithm>
#include <cmath>

class CameraControlSystem : public ISystem {
public:
    CameraControlSystem() { priority = -1; }

    void onUpdate(Registry& registry, float dt) override {
        const Entity e = CameraSystem::activeCamera(registry);
        if (e == NULL_ENTITY || !registry.hasComponent<CameraControl>(e)) return;
        Camera2d& camera = registry.getComponent<Camera2d>(e);
        CameraControl& control = registry.getComponent<CameraControl>(e);
        Transform2D& transform = registry.getComponent<Transform2D>(e);

        // Pan : vitesse constante à l'écran, quel que soit le zoom
        const bool following = registry.hasComponent<CameraFollow>(e)
            && registry.isValid(registry.getComponent<CameraFollow>(e).target);
        if (control.panEnabled && !following) {
            const sf::Vector2f dir = Math::clampLength(
                { Input::getAxis(control.actionX), Input::getAxis(control.actionY) }, 1.f);
            if (dir.x != 0.f || dir.y != 0.f)
                transform.setPosition(transform.getPosition() + dir * (control.panSpeed / camera.zoom * dt));
        }

        // Zoom : multiplicatif (même ressenti de près ou de loin), lissé
        if (control.targetZoom <= 0.f) control.targetZoom = camera.zoom;
        if (control.zoomEnabled) {
            const float held = Input::getAxis(control.actionZoom);
            const float notches = Input::getAxis(control.actionZoomWheel);
            control.targetZoom *= std::exp(held * control.zoomSpeed * dt) * std::pow(1.f + control.wheelStep, notches);
        }
        control.targetZoom = std::clamp(control.targetZoom, camera.minZoom, camera.maxZoom);
        camera.zoom = control.zoomSharpness > 0.f
            ? Math::damp(camera.zoom, control.targetZoom, control.zoomSharpness, dt)
            : control.targetZoom;
        if (std::abs(camera.zoom - control.targetZoom) < 1e-4f) camera.zoom = control.targetZoom;
    }
};
