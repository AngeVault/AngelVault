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

int main()
{
    Game game;

    game.systems().addSystem<PhysicsSystem>();
    game.systems().addSystem<CollisionSystem>();
    game.systems().addSystem<ScriptSystem>();
    game.systems().addSystem<CameraSystem>(game.window());

    Entity CameraEntity = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(CameraEntity).setPosition({ 200.f, 100.f });
    auto& camera = game.registry().addComponent<Camera2d>(CameraEntity);
    camera.setWorldBorder({0.f,0.f}, {3000.f,3000.f});
    game.registry().addComponent<NameComponent>(CameraEntity, "CameraEntity");

    Entity player = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(player).setPosition({ 200.f, 100.f });
    game.registry().addComponent<Geometry>(player, Geometry::SpriteData{ "player", sf::IntRect({0, 0 }, {32, 64}), sf::Color::White }, 1);
    game.registry().addComponent<RigidBody2D>(player).useGravity = false;
    auto& colPlayer = game.registry().addComponent<Collider2D>(player, Collider2D::makeBox({ 16.f, 32.f }));
    game.registry().addComponent<NameComponent>(player, "Player");

    if (game.registry().isValid(player)) camera.setTarget(player);

    // Attache le script PlayerBehavior
    auto& scriptPlayer = game.registry().addComponent<Script>(player);
    scriptPlayer.behavior = std::make_unique<PlayerBehavior>();

    game.run();
    return 0;
}