#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "components/buildingComp.h"
#include "RtsInput_state.h"
#include <SFML/Graphics.hpp>
#include <cfloat>
#include <cmath>
#include <algorithm>

class HealthBarSystem : public ISystem {
public:
    HealthBarSystem(sf::RenderWindow& window, sf::RenderTarget& target, InputState& input)
        : m_window(window), m_target(target), m_input(input) {
        priority = 13; 
    }

    void onUpdate(Registry& registry, float) override {
        sf::Vector2f mouseWorld = m_window.mapPixelToCoords(
            { int(m_input.mouseScreenPos.x), int(m_input.mouseScreenPos.y) });

        Entity hovered = NULL_ENTITY;
        float bestSq = FLT_MAX;
        registry.view<ECS::Unit, Transform2D>([&](Entity e, ECS::Unit& u, Transform2D& t) {
            if (u.pv <= 0) return;
            if (u.team == ECS::Unit::Team::Enemy && !u.visibleToPlayer) return;
            float dx = t.getPosition().x - mouseWorld.x;
            float dy = t.getPosition().y - mouseWorld.y;
            if (std::abs(dx) < 28.f && std::abs(dy) < 28.f) {
                float dSq = dx * dx + dy * dy;
                if (dSq < bestSq) { bestSq = dSq; hovered = e; }
            }
            });

        registry.view<ECS::Building, Transform2D>([&](Entity e, ECS::Building& building, Transform2D& t) {
            if (building.pv <= 0) return;
            const sf::Vector2f pos = t.getPosition();
            const float dx = pos.x - mouseWorld.x;
            const float dy = pos.y - mouseWorld.y;
            const sf::Vector2f scale = t.getScale();
            if (std::abs(dx) > building.halfSize.x * scale.x || std::abs(dy) > building.halfSize.y * scale.y) return;
            const float dSq = dx * dx + dy * dy;
            if (dSq < bestSq) { bestSq = dSq; hovered = e; }
        });

        if (hovered.index == UINT32_MAX) return; 

        sf::Vector2f pos = registry.getComponent<Transform2D>(hovered).getPosition();
        const bool isBuilding = registry.hasComponent<ECS::Building>(hovered);
        const int pv = isBuilding
            ? registry.getComponent<ECS::Building>(hovered).pv
            : registry.getComponent<ECS::Unit>(hovered).pv;
        const int pvMax = isBuilding
            ? registry.getComponent<ECS::Building>(hovered).pvMax
            : registry.getComponent<ECS::Unit>(hovered).pvMax;
        const float ratio = std::clamp(float(pv) / float(std::max(pvMax, 1)), 0.f, 1.f);
        const float barWidth = isBuilding ? 72.f : 40.f;
        // au dessus du batiment, 56 au minimum comme avant
        const float barOffset = isBuilding
            ? std::max(56.f, registry.getComponent<ECS::Building>(hovered).halfSize.y
                * registry.getComponent<Transform2D>(hovered).getScale().y + 10.f)
            : 26.f;
        const float fillWidth = barWidth - 4.f;

        m_back.setSize({ barWidth, 6.f });
        m_back.setOrigin({ barWidth * 0.5f, 3.f });
        m_back.setPosition({ pos.x, pos.y - barOffset });
        m_back.setFillColor(sf::Color(15, 15, 15, 220));
        m_target.draw(m_back);

        m_fill.setSize({ fillWidth * ratio, 4.f });
        m_fill.setOrigin({ 0.f, 2.f });
        m_fill.setPosition({ pos.x - fillWidth * 0.5f, pos.y - barOffset });
        m_fill.setFillColor(healthColor(ratio));
        m_target.draw(m_fill);
    }

private:
    sf::RenderWindow& m_window;
    sf::RenderTarget& m_target;
    InputState& m_input;
    sf::RectangleShape m_back; 
    sf::RectangleShape m_fill;

    static sf::Color healthColor(float r) {
        if (r > 0.5f)  return sf::Color(80, 230, 120);  
        if (r > 0.25f) return sf::Color(255, 210, 80); 
        return sf::Color(230, 70, 70);            
    }
};
