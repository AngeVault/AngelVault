#pragma once

#include "../components/buildingComp.h"
#include "../components/selectable.h"
#include "../RtsInput_state.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

// zone de visee autour des batiments : vert et qui pulse quand ils sont selectionnes,
// blanc plus discret quand le curseur passe dessus (coins + petits reperes au milieu des cotes)
class BuildingHighlightSystem : public ISystem {
public:
    BuildingHighlightSystem(sf::RenderWindow& window, sf::RenderTarget& target, InputState& input)
        : m_window(window), m_target(target), m_input(input) {
        priority = 12; // avec les autres marqueurs de selection
    }

    void onUpdate(Registry& registry, float dt) override {
        m_time += dt;
        const sf::Vector2f mouse = m_window.mapPixelToCoords(
            { static_cast<int>(m_input.mouseScreenPos.x), static_cast<int>(m_input.mouseScreenPos.y) });

        // un seul batiment survole : le plus proche du curseur
        Entity hovered = NULL_ENTITY;
        float bestDistance = 1e12f;
        registry.view<ECS::Building, Transform2D>([&](Entity e, ECS::Building& building, Transform2D& transform) {
            if (building.pv <= 0) return;
            const sf::Vector2f half = scaledHalf(building, transform);
            const sf::Vector2f position = transform.getPosition();
            if (std::abs(mouse.x - position.x) > half.x || std::abs(mouse.y - position.y) > half.y) return;
            const float distance = std::hypot(mouse.x - position.x, mouse.y - position.y);
            if (distance < bestDistance) { bestDistance = distance; hovered = e; }
        });

        registry.view<ECS::Building, ECS::Selectable, Transform2D>(
            [&](Entity e, ECS::Building& building, ECS::Selectable& selectable, Transform2D& transform) {
                if (building.pv <= 0 || !selectable.playerOwned) return;
                const bool isHovered = e == hovered;
                if (!selectable.selected && !isHovered) return;
                draw(transform.getPosition(), scaledHalf(building, transform), selectable.selected);
            });
    }

private:
    sf::RenderWindow& m_window;
    sf::RenderTarget& m_target;
    InputState& m_input;
    float m_time = 0.f;

    // taille actuelle (le batiment grandit pendant sa construction)
    static sf::Vector2f scaledHalf(const ECS::Building& building, const Transform2D& transform) {
        const sf::Vector2f scale = transform.getScale();
        return { std::max(10.f, building.halfSize.x * scale.x), std::max(10.f, building.halfSize.y * scale.y) };
    }

    void fillRect(sf::Vector2f position, sf::Vector2f size, sf::Color color) {
        sf::RectangleShape shape(size);
        shape.setPosition(position);
        shape.setFillColor(color);
        m_target.draw(shape);
    }

    void draw(sf::Vector2f center, sf::Vector2f half, bool selected) {
        const sf::Color line = selected ? sf::Color(80, 230, 120, 255) : sf::Color(255, 240, 248, 215);
        const sf::Color fill = selected ? sf::Color(80, 230, 120, 30) : sf::Color(255, 230, 240, 22);
        const float pulse = selected ? std::sin(m_time * 4.f) : 0.f;
        const float pad = 6.f + pulse * 2.f;       // la zone respire quand c'est selectionne
        const float thick = selected ? 3.f : 2.f;

        const sf::Vector2f topLeft = center - half - sf::Vector2f(pad, pad);
        const sf::Vector2f size = (half + sf::Vector2f(pad, pad)) * 2.f;
        fillRect(topLeft, size, fill);

        // coins
        const float arm = std::clamp(std::min(size.x, size.y) * 0.28f, 10.f, 26.f);
        const sf::Vector2f right = topLeft + sf::Vector2f(size.x, 0.f);
        const sf::Vector2f bottom = topLeft + sf::Vector2f(0.f, size.y);
        const sf::Vector2f corner = topLeft + size;
        fillRect(topLeft, { arm, thick }, line);                                 // haut gauche
        fillRect(topLeft, { thick, arm }, line);
        fillRect(right - sf::Vector2f(arm, 0.f), { arm, thick }, line);          // haut droite
        fillRect(right - sf::Vector2f(thick, 0.f), { thick, arm }, line);
        fillRect(bottom - sf::Vector2f(0.f, thick), { arm, thick }, line);       // bas gauche
        fillRect(bottom - sf::Vector2f(0.f, arm), { thick, arm }, line);
        fillRect(corner - sf::Vector2f(arm, thick), { arm, thick }, line);       // bas droite
        fillRect(corner - sf::Vector2f(thick, arm), { thick, arm }, line);

        // petits reperes qui pointent vers le batiment au milieu de chaque cote
        const float tick = selected ? 8.f : 5.f;
        fillRect({ center.x - thick * 0.5f, topLeft.y - tick }, { thick, tick }, line);
        fillRect({ center.x - thick * 0.5f, corner.y }, { thick, tick }, line);
        fillRect({ topLeft.x - tick, center.y - thick * 0.5f }, { tick, thick }, line);
        fillRect({ corner.x, center.y - thick * 0.5f }, { tick, thick }, line);
    }
};
