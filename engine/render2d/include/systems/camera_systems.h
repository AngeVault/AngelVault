#pragma once
// Tous les systèmes caméra, dans le bon ordre (priorités : voir camera_system.h)
#include "ecs/system_manager.h"
#include "systems/camera_bounds_system.h"
#include "systems/camera_control_system.h"
#include "systems/camera_follow_system.h"
#include "systems/camera_shake_system.h"
#include "systems/camera_system.h"

inline void registerCameraSystems(SystemManager& systems, sf::RenderTarget& target) {
    systems.addSystem<CameraControlSystem>();
    systems.addSystem<CameraFollowSystem>();
    systems.addSystem<CameraBoundsSystem>(target);
    systems.addSystem<CameraShakeSystem>();
    systems.addSystem<CameraSystem>(target);
}
