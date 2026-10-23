#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/rigid_body2d.h"
#include "components/sprite_component.h"
#include <SFML/Graphics.hpp>
#include <cmath>

class ForceDebugSystem : public ISystem {
public:
    explicit ForceDebugSystem(sf::RenderWindow& window) : m_window(window) {
        priority = 12;
    }

    float scale = 0.12f; // pixels par unité de force — plus grand = flèches plus longues
    float thickness = 2.f;  
    bool  showVelocity = true;
    bool  showGravity = true;
    bool  showForces = true;

    void onUpdate(Registry& registry, float dt) override {
        registry.view<RigidBody2D, Transform2D>(
            [&](Entity e, RigidBody2D& rb, Transform2D& t) {
                if (rb.isStatic) return;

             
                sf::Vector2f origin = t.getPosition();
                if (registry.hasComponent<SpriteComponent>(e)) {
                    auto& sc = registry.getComponent<SpriteComponent>(e);
                    auto  bounds = sc.sprite.getLocalBounds();
                    origin += sf::Vector2f{ bounds.size.x * 0.5f, bounds.size.y * 0.5f };
                }

                if (showVelocity)
                    drawArrow(origin, rb.velocity * scale,
                        sf::Color(80, 180, 255, 220));  // bleu  = vitesse

                if (showGravity && rb.useGravity)
                    drawArrow(origin, sf::Vector2f{ 0.f, 980.f * rb.mass * scale * 0.15f },
                        sf::Color(255, 80, 80, 200));   // rouge = gravité

                if (showForces) {
                    sf::Vector2f f = rb.forceAccumulator * scale * 0.1f;
                    drawArrow(origin, f,
                        sf::Color(80, 255, 120, 200));  // vert  = forces custom
                }

                sf::CircleShape dot(4.f);
                dot.setOrigin({ 4.f, 4.f });
                dot.setPosition(origin + sf::Vector2f{ 0.f, 20.f });
                dot.setFillColor(rb.isGrounded
                    ? sf::Color(80, 255, 80, 200)    // vert = au sol
                    : sf::Color(255, 80, 80, 200));   // rouge = en l'air
                m_window.draw(dot);
            });
    }

private:
    sf::RenderWindow& m_window;

    void drawArrow(sf::Vector2f origin, sf::Vector2f vec, sf::Color color) {
        float len = std::hypot(vec.x, vec.y);
        if (len < 4.f) return;

        float angle = std::atan2(vec.y, vec.x);
        float headLen = std::min(len * 0.35f, 18.f);
        float bodyLen = len - headLen;

        // corps fleche
        sf::RectangleShape body({ bodyLen, thickness });
        body.setOrigin({ 0.f, thickness * 0.5f });
        body.setPosition(origin);
        body.setRotation(sf::radians(angle));
        body.setFillColor(color);
        m_window.draw(body);

        // teuteu flèche 
        sf::Vector2f tip = origin + vec;
        sf::Vector2f baseL = {
            tip.x - headLen * std::cos(angle - 0.45f),
            tip.y - headLen * std::sin(angle - 0.45f)
        };
        sf::Vector2f baseR = {
            tip.x - headLen * std::cos(angle + 0.45f),
            tip.y - headLen * std::sin(angle + 0.45f)
        };

        sf::ConvexShape head(3);
        head.setPoint(0, tip);
        head.setPoint(1, baseL);
        head.setPoint(2, baseR);
        head.setFillColor(color);
        m_window.draw(head);
    }
};