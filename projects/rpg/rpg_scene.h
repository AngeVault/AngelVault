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
#include "systems/tile_collision_system.h"
#include "systems/script_system.h"
#include "components/name.h"
#include "components/geometry.h"
#include "systems/camera_systems.h"
#include "components/camera2d.h"
#include "components/camera_bounds.h"
#include "components/camera_control.h"
#include "components/camera_follow.h"
#include "components/camera_shake.h"
#include "rpg_game_code.h"
#include "input/input_manager.h"

inline void loadRpgEntities(Registry& registry) {
    Entity cameraEntity = registry.createEntity();
    registry.addComponent<Transform2D>(cameraEntity).setPosition({ 200.f, 100.f });
    registry.addComponent<Camera2d>(cameraEntity);
    registry.addComponent<CameraBounds>(cameraEntity).set({ 0.f, 0.f }, { 3000.f, 3000.f });
    registry.addComponent<CameraControl>(cameraEntity).panEnabled = false;   
    registry.addComponent<CameraShake>(cameraEntity);
    registry.addComponent<NameComponent>(cameraEntity, "CameraEntity");

    Entity player = registry.createEntity();
    registry.addComponent<Transform2D>(player).setPosition({ 200.f, 100.f });
    registry.addComponent<Geometry>(player, Geometry::SpriteData{ "player", sf::IntRect({ 0, 0 }, { 32, 64 }), sf::Color::White }, 1);
    registry.addComponent<RigidBody2D>(player).useGravity = false;
    registry.addComponent<Collider2D>(player, Collider2D::makeBox({ 16.f, 32.f }));
    registry.addComponent<NameComponent>(player, "Player");

    auto& follow = registry.addComponent<CameraFollow>(cameraEntity);
    follow.setTarget(player);
    follow.lookAhead = 80.f;

    registry.addComponent<Script>(player).add<PlayerBehavior>();
}

inline void defaultRpgInput(InputManager& input) {
    using Key = sf::Keyboard::Key;
    using Src = InputBinding::Source;
    input.clearActions();

    auto& h = input.addAction("Horizontal", InputAction::Type::Axis);
    h.bindings = { { Src::Key, int(Key::Left), -1.f }, { Src::Key, int(Key::Right), 1.f },
                   { Src::Key, int(Key::Q), -1.f },    { Src::Key, int(Key::A), -1.f },
                   { Src::Key, int(Key::D), 1.f },     { Src::JoystickAxis, int(sf::Joystick::Axis::X), 1.f, 0 } };
    auto& v = input.addAction("Vertical", InputAction::Type::Axis);
    v.bindings = { { Src::Key, int(Key::Up), -1.f },   { Src::Key, int(Key::Down), 1.f },
                   { Src::Key, int(Key::Z), -1.f },    { Src::Key, int(Key::W), -1.f },
                   { Src::Key, int(Key::S), 1.f },     { Src::JoystickAxis, int(sf::Joystick::Axis::Y), 1.f, 0 } };

    auto& zoom = input.addAction("Zoom", InputAction::Type::Axis);
    zoom.bindings = { { Src::Key, int(Key::Add), 1.f },   { Src::Key, int(Key::Subtract), -1.f },
                      { Src::Key, int(Key::Equal), 1.f }, { Src::Key, int(Key::Hyphen), -1.f } };
    auto& wheel = input.addAction("ZoomWheel", InputAction::Type::Axis);  
    wheel.bindings = { { Src::MouseWheel, 0, 1.f } };

    auto& camX = input.addAction("CameraX", InputAction::Type::Axis);  
    camX.bindings = { { Src::Key, int(Key::J), -1.f }, { Src::Key, int(Key::L), 1.f } };
    auto& camY = input.addAction("CameraY", InputAction::Type::Axis);
    camY.bindings = { { Src::Key, int(Key::I), -1.f }, { Src::Key, int(Key::K), 1.f } };
}

inline void loadRpgInput(InputManager& input, const std::string& path = "assets/input.cfg") {
    if (!input.loadFromFile(path)) defaultRpgInput(input);
}

inline void registerRpgSystems(Game& game) {
    game.systems().addSystem<TileCollisionSystem>();  
    game.systems().addSystem<PhysicsSystem>();
    game.systems().addSystem<CollisionSystem>();
    game.systems().addSystem<ScriptSystem>();
    registerCameraSystems(game.systems(), game.target());
}

inline void loadRpgScene(Game& game) {
    loadRpgInput(game.input());
    registerRpgSystems(game);
    loadRpgEntities(game.registry());
}
