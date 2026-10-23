#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "components/selectable.h"
#include "components/rtsPlayerState.h"
#include "RtsInput_state.h"
#include "../utils/unit_formation.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

class SelectionRenderSystem : public ISystem {
public:
    SelectionRenderSystem(sf::RenderWindow& window, sf::RenderTarget& target, InputState& input)
        : m_window(window), m_target(target), m_input(input) {
        priority = 12;
    }

    void onUpdate(Registry& registry, float /*dt*/) override {
        registry.view<ECS::Selectable, Transform2D>(
            [&](Entity, ECS::Selectable& selectable, Transform2D& t) {
                if (!selectable.playerOwned || !selectable.selected) return;
                sf::CircleShape ring(22.f);
                ring.setOrigin({ 22.f, 22.f });
                ring.setPosition(t.getPosition() + sf::Vector2f(0.f, 16.f));
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(sf::Color(80, 230, 120));
                ring.setOutlineThickness(2.f);
                m_target.draw(ring);
            });

        auto toWorld = [&](sf::Vector2f screen)
            {
                return m_window.mapPixelToCoords(
                    { int(screen.x), int(screen.y) });
            };

        // Boite de drag
        if (m_input.isDragging) {
            sf::Vector2f a = toWorld(m_input.dragStart);
            sf::Vector2f b = toWorld(m_input.mouseScreenPos);
            sf::RectangleShape rect;
            rect.setPosition({ std::min(a.x, b.x), std::min(a.y, b.y) });
            rect.setSize({ std::abs(b.x - a.x), std::abs(b.y - a.y) });
            rect.setFillColor(sf::Color(80, 230, 120, 40));
            rect.setOutlineColor(sf::Color(80, 230, 120));
            rect.setOutlineThickness(1.f);
            m_target.draw(rect);
        }

        sf::VertexArray lines(sf::PrimitiveType::Lines);
        registry.view<ECS::Unit, ECS::Selectable, Transform2D>(
            [&](Entity, ECS::Unit& u, ECS::Selectable& selectable, Transform2D& t) {
                if (u.team != ECS::Unit::Team::Player || !selectable.selected) return;

                if (u.target.index != UINT32_MAX) {
                    const Entity tgt = u.target;
                    if (registry.isValid(tgt) && registry.hasComponent<Transform2D>(tgt)) {
                        if (registry.hasComponent<ECS::Unit>(tgt)) {
                            const ECS::Unit& targetUnit = registry.getComponent<ECS::Unit>(tgt);
                            if (targetUnit.team == ECS::Unit::Team::Enemy
                                && !targetUnit.visibleToPlayer) return;
                        }
                        sf::Vector2f tp = registry.getComponent<Transform2D>(tgt).getPosition();
                        lines.append(sf::Vertex{ t.getPosition(), sf::Color(230, 80, 80, 210) });
                        lines.append(sf::Vertex{ tp, sf::Color(230, 80, 80, 210) });
                    }
                }
                else if (u.hasDestination) {
                    lines.append(sf::Vertex{ t.getPosition(), sf::Color(120, 230, 150, 210) });
                    lines.append(sf::Vertex{ u.destination, sf::Color(120, 230, 150, 210) });
                }
            });
        m_target.draw(lines);

        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            if (!hud.formationPreviewActive || hud.formationPreviewUnitCount == 0) return;
            const auto positions = hud.formationPreviewPositions.empty()
                ? Rts::formationPositions(hud.formationPreviewCenter,
                    hud.formationPreviewEnd, hud.formationPreviewUnitCount)
                : hud.formationPreviewPositions;
            if (positions.empty()) return;
            std::vector<sf::Vector2f> hullPoints = positions;
            std::sort(hullPoints.begin(), hullPoints.end(), [](sf::Vector2f a, sf::Vector2f b) {
                return a.x == b.x ? a.y < b.y : a.x < b.x;
            });
            auto cross = [](sf::Vector2f origin, sf::Vector2f a, sf::Vector2f b) {
                return (a.x - origin.x) * (b.y - origin.y)
                    - (a.y - origin.y) * (b.x - origin.x);
            };
            std::vector<sf::Vector2f> hull;
            for (const sf::Vector2f point : hullPoints) {
                while (hull.size() >= 2
                    && cross(hull[hull.size() - 2], hull.back(), point) <= 0.f)
                    hull.pop_back();
                hull.push_back(point);
            }
            const std::size_t lowerHullSize = hull.size();
            for (std::size_t i = hullPoints.size() - 1; i > 0; --i) {
                const sf::Vector2f point = hullPoints[i - 1];
                while (hull.size() > lowerHullSize
                    && cross(hull[hull.size() - 2], hull.back(), point) <= 0.f)
                    hull.pop_back();
                hull.push_back(point);
            }
            if (hull.size() > 1) hull.pop_back();
            if (!hull.empty()) {
                sf::VertexArray outline(sf::PrimitiveType::LineStrip);
                for (const sf::Vector2f point : hull)
                    outline.append(sf::Vertex{ point, sf::Color(237, 127, 179, 190) });
                outline.append(sf::Vertex{ hull.front(), sf::Color(237, 127, 179, 190) });
                m_target.draw(outline);
            }
            for (const sf::Vector2f position : positions) {
                sf::CircleShape marker(12.f);
                marker.setOrigin({ 12.f, 12.f });
                marker.setPosition(position);
                marker.setFillColor(sf::Color(255, 205, 227, 180));
                marker.setOutlineColor(sf::Color(255, 242, 249, 240));
                marker.setOutlineThickness(2.f);
                m_target.draw(marker);
            }
        });
    }

private:
    sf::RenderWindow& m_window;
    sf::RenderTarget& m_target;
    InputState& m_input;
};
