#pragma once
#include "ecs/behavior.h"
#include "ecs/behavior_factory.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/geometry.h"
#include "components/camera2d.h"
#include "input/input.h"
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

class bouton : public IBehavior {
public:
    PROPERTY(bool, enabled, true)                               
    PROPERTY(float, hoverLighten, 0.20f)                    
    PROPERTY(float, pressDarken, 0.25f)                          
    PROPERTY(sf::Vector2f, size, sf::Vector2f(200.f, 60.f))       
    PROPERTY(sf::Vector2f, viewSize, sf::Vector2f(1920.f, 1080.f)) 
    PROPERTY(std::string, logMessage, std::string("Bouton clique !")) 

    std::function<void(Entity, Registry&)> onClick;       
    std::function<void(Entity, Registry&)> onHoverEnter;   
    std::function<void(Entity, Registry&)> onHoverExit;   

    // Lecture à la demande (sans callback)
    bool isHovered() const { return m_hovered; }
    bool isPressed() const { return m_pressed && m_hovered; }
    bool wasClicked() const { return m_clicked; } 

    void onStart(Entity self, Registry& registry) override {
        if (registry.hasComponent<Geometry>(self)) {
            m_baseColor = shapeColor(registry.getComponent<Geometry>(self));
            m_hasBaseColor = true;
        }
    }

    void onUpdate(Entity self, Registry& registry, float dt) override {
        m_clicked = false;
        if (!registry.hasComponent<Transform2D>(self)) return;

        if (!enabled) {
            m_pressed = false;
            m_hovered = false;
            refreshColor(self, registry);
            return;
        }

        const sf::Vector2f world = screenToWorld(registry, Input::mousePosition());
        const bool hovered = contains(self, registry, world);

        if (hovered != m_hovered) {
            m_hovered = hovered;
            if (m_hovered) { if (onHoverEnter) onHoverEnter(self, registry); }
            else           { if (onHoverExit)  onHoverExit(self, registry); }
        }

        const InputManager* input = InputManager::current();
        if (input && input->getMouseButtonDown(sf::Mouse::Button::Left) && m_hovered)
            m_pressed = true;

        if (m_pressed && !Input::getMouseButton(sf::Mouse::Button::Left)) {
            m_pressed = false;
            if (m_hovered) {
                m_clicked = true;
                if (!logMessage.empty()) std::cout << "[bouton] " << logMessage << "\n";
                if (onClick) onClick(self, registry);
            }
        }

        refreshColor(self, registry);
    }

    void onDestroy(Entity self, Registry& registry) override {
        m_hovered = m_pressed = false;
        if (m_hasBaseColor && registry.isValid(self) && registry.hasComponent<Geometry>(self))
            setShapeColor(registry.getComponent<Geometry>(self), m_baseColor);
    }

private:
    sf::Vector2f screenToWorld(Registry& registry, sf::Vector2i pixel) const {
        sf::Vector2f result(float(pixel.x), float(pixel.y));
        bool found = false;
        registry.view<Camera2d, Transform2D>([&](Entity, Camera2d& camera, Transform2D& transform) {
            if (found) return;
            found = true;
            const float zoom = camera.zoom > 0.f ? camera.zoom : 1.f;
            result = transform.getPosition() + sf::Vector2f(
                (float(pixel.x) - viewSize.x * 0.5f) / zoom,
                (float(pixel.y) - viewSize.y * 0.5f) / zoom);
        });
        return result;
    }

    bool contains(Entity self, Registry& registry, sf::Vector2f world) const {
        const Transform2D& transform = registry.getComponent<Transform2D>(self);
        const sf::Vector2f p = transform.getLocalMatrix().getInverse().transformPoint(world);

        if (!registry.hasComponent<Geometry>(self))
            return std::abs(p.x) <= size.x * 0.5f && std::abs(p.y) <= size.y * 0.5f;

        const Geometry& geometry = registry.getComponent<Geometry>(self);
        return std::visit([&](const auto& shape) -> bool {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, Geometry::BoxData>) {
                return std::abs(p.x) <= shape.halfSize.x && std::abs(p.y) <= shape.halfSize.y;
            }
            else if constexpr (std::is_same_v<T, Geometry::CircleData>) {
                return p.x * p.x + p.y * p.y <= shape.radius * shape.radius;
            }
            else if constexpr (std::is_same_v<T, Geometry::SpriteData>) {
                return std::abs(p.x) <= shape.rect.size.x * 0.5f && std::abs(p.y) <= shape.rect.size.y * 0.5f;
            }
            else {  
                const auto& pts = shape.points;
                bool inside = false;
                for (std::size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
                    if (((pts[i].y > p.y) != (pts[j].y > p.y)) &&
                        (p.x < (pts[j].x - pts[i].x) * (p.y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x))
                        inside = !inside;
                }
                return inside;
            }
        }, geometry.data);
    }

    static sf::Color shapeColor(Geometry& geometry) {
        return std::visit([](auto& shape) -> sf::Color {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, Geometry::SpriteData>) return shape.tint;
            else return shape.style.fillColor;
        }, geometry.data);
    }

    static void setShapeColor(Geometry& geometry, sf::Color color) {
        std::visit([&](auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, Geometry::SpriteData>) shape.tint = color;
            else shape.style.fillColor = color;
        }, geometry.data);
    }

    static sf::Color mix(sf::Color a, sf::Color b, float t) {
        t = std::clamp(t, 0.f, 1.f);
        auto channel = [t](std::uint8_t x, std::uint8_t y) {
            return static_cast<std::uint8_t>(float(x) + (float(y) - float(x)) * t + 0.5f);
        };
        return sf::Color(channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), a.a);
    }

    void refreshColor(Entity self, Registry& registry) {
        if (!registry.hasComponent<Geometry>(self)) return;
        Geometry& geometry = registry.getComponent<Geometry>(self);
        if (!m_hasBaseColor) {   // Geometry ajoutée après onStart
            m_baseColor = shapeColor(geometry);
            m_hasBaseColor = true;
        }
        sf::Color color = m_baseColor;
        if (enabled && m_hovered) color = mix(m_baseColor, sf::Color::White, hoverLighten);
        if (enabled && m_hovered && m_pressed) color = mix(m_baseColor, sf::Color::Black, pressDarken);
        if (!enabled) color = mix(m_baseColor, sf::Color(128, 128, 128, m_baseColor.a), 0.6f);   // grisé
        setShapeColor(geometry, color);
    }

    bool m_hovered = false;
    bool m_pressed = false;
    bool m_clicked = false;
    bool m_hasBaseColor = false;
    sf::Color m_baseColor = sf::Color::White;
};

REGISTER_BEHAVIOR(bouton)