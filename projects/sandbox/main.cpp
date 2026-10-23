#include "game_loop.h"
#include "utils/texture_factory.h"
#include "components/transform2d.h"
#include "components/sprite_component.h"
#include "components/rigid_body2d.h"
#include "components/collider2d.h"
#include "components/script_component.h"
#include "systems/physics_system.h"
#include "systems/collision_system.h"
#include "systems/script_system.h"
#include "behaviors/player_behavior.h"
#include "components/name.h"
#include "behaviors/rotating.h"
#include "components/geometry.h"


int main() {
    Game game;

    game.systems().addSystem<PhysicsSystem>();
    game.systems().addSystem<CollisionSystem>();
    game.systems().addSystem<ScriptSystem>();

    Entity player = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(player).setPosition({ 200.f, 100.f });
    game.registry().addComponent<Geometry>(player, Geometry::SpriteData{ "player", sf::IntRect({0, 0 }, {32, 64}), sf::Color::White }, 1);
    game.registry().addComponent<RigidBody2D>(player);
    auto& colPlayer = game.registry().addComponent<Collider2D>(player, Collider2D::makeBox({ 16.f, 32.f }));
	game.registry().addComponent<NameComponent>(player, "Player");

    // Attache le script PlayerBehavior
    auto& scriptPlayer = game.registry().addComponent<Script>(player);
    scriptPlayer.behavior = std::make_unique<PlayerBehavior>();

    // sol
    Entity ground = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(ground).setPosition({ 960.f, 550.f }); 
	game.registry().addComponent<Geometry>(ground, Geometry::BoxData{ { 960.f, 8.f }, { sf::Color(100, 100, 100), sf::Color::Transparent, 0.f, "" } }, 0);
    game.registry().addComponent<RigidBody2D>(ground).isStatic = true;
    auto& colGround = game.registry().addComponent<Collider2D>(ground, Collider2D::makeBox({ 960.f, 8.f }));
	game.registry().addComponent<NameComponent>(ground, "Ground");

    // mur droit
    Entity wall = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(wall).setPosition({ 1900.f, 275.f});
	game.registry().addComponent<Geometry>(wall, Geometry::BoxData{ { 8.f, 275.f }, { sf::Color(100, 100, 100), sf::Color::Transparent, 0.f, "" } }, 0);
	game.registry().addComponent<RigidBody2D>(wall).isStatic = true;
	auto& colWall = game.registry().addComponent<Collider2D>(wall, Collider2D::makeBox({ 8.f, 275.f }));
	game.registry().addComponent<NameComponent>(wall, "Wall");

    // balle
    Entity ball = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(ball).setPosition({ 400.f, 200.f });
	game.registry().addComponent<Geometry>(ball, Geometry::CircleData{ 16.f, { sf::Color(255, 0, 0), sf::Color::Transparent, 0.f, "" } }, 1);
    game.registry().addComponent<RigidBody2D>(ball).mass = 100.0f;
    auto& colBall = game.registry().addComponent<Collider2D>(ball, Collider2D::makeCircle(16.f));
    colBall.restitution = 0.5f;
	game.registry().addComponent<NameComponent>(ball, "Ball");

    // care volant 
    Entity car = game.registry().createEntity();
    game.registry().addComponent<Transform2D>(car).setPosition({ 300.f, 300.f });
	game.registry().addComponent<Geometry>(car, Geometry::BoxData{ { 16.f, 16.f }, { sf::Color(0, 255, 0), sf::Color::Transparent, 0.f, "" } }, 1);
    auto& rbCar = game.registry().addComponent<RigidBody2D>(car);
    rbCar.useGravity = false;
    auto& colCar = game.registry().addComponent<Collider2D>(car, Collider2D::makeBox(sf::Vector2f(16.f, 16.f)));
    colCar.restitution = 0.f;
    game.registry().addComponent<NameComponent>(car, "carrer qui tourne");
    auto& carBehavior = game.registry().addComponent<Script>(car);
    carBehavior.behavior = std::make_unique<Rotation>();

    game.run();
    return 0;
}