#pragma once
// Scène du projet RPG, partagée entre rpg.exe et l'éditeur (comme sandbox_scene.h pour le Sandbox).
// Ajoutez ici les entités et les systèmes de jeu : les deux programmes les verront.
#include "game_loop.h"
#include "components/transform2d.h"
#include "components/rigid_body2d.h"
#include "components/collider2d.h"
#include "components/script_component.h"
#include "systems/physics_system.h"
#include "systems/collision_system.h"
#include "systems/script_system.h"
#include "components/name.h"
#include "components/geometry.h"
#include "systems/camera_system.h"
#include "components/camera2d.h"
#include "behaviors/player_behaviors.h"

// Entités de la scène. Séparé des systèmes pour que l'éditeur puisse charger
// la scène dans son propre Registry (Hierarchy / Inspector) sans lancer le jeu.
inline void loadRpgEntities(Registry& registry) {
    Entity cameraEntity = registry.createEntity();
    registry.addComponent<Transform2D>(cameraEntity).setPosition({ 200.f, 100.f });
    auto& camera = registry.addComponent<Camera2d>(cameraEntity);
    camera.setWorldBorder({ 0.f, 0.f }, { 3000.f, 3000.f });
    registry.addComponent<NameComponent>(cameraEntity, "CameraEntity");

    Entity player = registry.createEntity();
    registry.addComponent<Transform2D>(player).setPosition({ 200.f, 100.f });
    registry.addComponent<Geometry>(player, Geometry::SpriteData{ "player", sf::IntRect({ 0, 0 }, { 32, 64 }), sf::Color::White }, 1);
    registry.addComponent<RigidBody2D>(player).useGravity = false;
    registry.addComponent<Collider2D>(player, Collider2D::makeBox({ 16.f, 32.f }));
    registry.addComponent<NameComponent>(player, "Player");

    // La caméra suit le joueur
    registry.getComponent<Camera2d>(cameraEntity).setTarget(player);

    // Attache le script PlayerBehavior
    auto& scriptPlayer = registry.addComponent<Script>(player);
    scriptPlayer.behavior = std::make_unique<PlayerBehavior>();
}

// Scène complète pour jouer : systèmes de jeu + entités
inline void loadRpgScene(Game& game) {
    game.systems().addSystem<PhysicsSystem>();
    game.systems().addSystem<CollisionSystem>();
    game.systems().addSystem<ScriptSystem>();
    game.systems().addSystem<CameraSystem>(game.window());
    loadRpgEntities(game.registry());
}
