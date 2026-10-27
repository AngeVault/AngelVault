//
// Created by p.pagliazzo on 27/10/2026.
//
#include "ecs/behavior.h"
#include "ecs/registry.h"
#include "components/rigid_body2d.h"
#include <SFML/Window/Keyboard.hpp>
#include "components/geometry.h"
#pragma once

class PlayerBehavior : public IBehavior {
public:
    PROPERTY(float, speed, 250.f)

        void onUpdate(Entity self, Registry& registry, float dt) override
        {

        keyTimer += dt;

        auto& rb = registry.getComponent<RigidBody2D>(self);

        float moveX = 0.f;
        float moveY = 0.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
        {
            moveX = -1.f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        {
            moveX = 1.f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        {
            moveY = -1.f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
        {
            moveY = 1.f;
        }

        rb.velocity.x = moveX * speed;
        rb.velocity.y = moveY * speed;
    }

private:
    float keyTimer = 0;
};
