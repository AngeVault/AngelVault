#pragma once
// Raccourcis caméra pour le code de jeu (scripts, systèmes) :
//   Camera::shake(registry, 0.5f);                          // fait trembler la caméra affichée
//   Camera::follow(registry, camera, player);               // la caméra suit le joueur
//   sf::Vector2f p = Camera::screenToWorld(Input::mousePosition());
#include "ecs/registry.h"
#include "components/camera2d.h"
#include "components/camera_follow.h"
#include "components/camera_shake.h"
#include "systems/camera_system.h"

namespace Camera {

inline Entity active(Registry& registry) { return CameraSystem::activeCamera(registry); }

// Ajoute du trauma (0..1) ; caméra affichée par défaut. Ajoute CameraShake si besoin.
inline void shake(Registry& registry, float trauma, Entity camera = NULL_ENTITY) {
    if (camera == NULL_ENTITY) camera = active(registry);
    if (!registry.isValid(camera)) return;
    if (!registry.hasComponent<CameraShake>(camera)) registry.addComponent<CameraShake>(camera);
    registry.getComponent<CameraShake>(camera).add(trauma);
}

// Suivre `target` (NULL_ENTITY = libre). Ajoute CameraFollow si besoin.
inline void follow(Registry& registry, Entity camera, Entity target) {
    if (!registry.isValid(camera)) return;
    if (!registry.hasComponent<CameraFollow>(camera)) registry.addComponent<CameraFollow>(camera);
    registry.getComponent<CameraFollow>(camera).setTarget(target);
}

inline sf::Vector2f screenToWorld(sf::Vector2i pixel) { return CameraSystem::screenToWorld(pixel); }
inline sf::Vector2i worldToScreen(sf::Vector2f point) { return CameraSystem::worldToScreen(point); }

} // namespace Camera
