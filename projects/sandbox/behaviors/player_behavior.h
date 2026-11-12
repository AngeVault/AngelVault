#pragma once
#include "ecs/behavior.h"
#include "ecs/registry.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include <SFML/Window/Keyboard.hpp>
#include "utils/texture_factory.h"
#include "components/geometry.h"

inline void shootBall(Registry& registry, int ballId, Entity self)
{
    auto& playerTrans = registry.getComponent<Transform2D>(self);
    auto& rbPlayer = registry.getComponent<RigidBody2D>(self);
    
    sf::Vector2f spawnPose = playerTrans.getPosition();
    spawnPose.x += 33;
    spawnPose.y += 1;

    Entity ball = registry.createEntity();
    registry.addComponent<Transform2D>(ball).setPosition( spawnPose);
	registry.addComponent<Geometry>(ball, Geometry::CircleData{ 16.f, { sf::Color(255, 0, 0), sf::Color::Transparent, 0.f, "ball" } }, 1);
    auto& rbBall = registry.addComponent<RigidBody2D>(ball);
    auto& colBall = registry.addComponent<Collider2D>(ball, Collider2D::makeCircle(16.f));
    colBall.restitution = 0.5f;
    rbBall.mass = 0.5f;
    rbBall.applyImpulse({ 50  + rbPlayer.velocity.x * 2 , 0 + rbPlayer.velocity.y * 2 });
 
    char ballName[10];
    std::snprintf(ballName, sizeof(ballName), "ball %d", ballId); 
    registry.addComponent<NameComponent>(ball, ballName);

}

class PlayerBehavior : public IBehavior {
public:
    PROPERTY(float, speed, 250.f)
    PROPERTY(float, jumpForce, 550.f)
    PROPERTY(float, groundFriction, 0.75f)
    PROPERTY(bool, canDoubleJump, true)
    PROPERTY(int, maxAmmo, 10)
    PROPERTY(int, ballId, 1)


        void onUpdate(Entity self, Registry& registry, float dt) override 
        {

        keyTimer += dt;

        auto& rb = registry.getComponent<RigidBody2D>(self);

        float moveX = 0.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
        {
            moveX = -1.f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        {
            moveX = 1.f;
        }

        rb.velocity.x = moveX * speed;

        bool jumpPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z);

        if (jumpPressed && !m_jumpHeld) 
        {
            if (rb.isGrounded) {
                rb.applyImpulse({ 0.f, -jumpForce });
                m_jumpsLeft = canDoubleJump ? 1 : 0;
                m_jumpHeld = true;
            }
            else if (m_jumpsLeft > 0) {
                rb.velocity.y = 0.f;
                rb.applyImpulse({ 0.f, -jumpForce });
                m_jumpsLeft--;
                m_jumpHeld = true;
            }
        }

        bool ePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);

        if (ePressed && keyTimer > 2.0f)
        {
            shootBall(registry, ballId, self);
            keyTimer = 0;
        }


        if (!jumpPressed) m_jumpHeld = false;

        if (moveX == 0.f && rb.isGrounded)
            rb.velocity.x *= groundFriction;
    }

private:
    bool  m_jumpHeld = false;
    int   m_jumpsLeft = 0;
    float keyTimer = 0;
};