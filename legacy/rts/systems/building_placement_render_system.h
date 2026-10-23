#pragma once

#include "../RtsInput_state.h"
#include "../RtsTile_map.hpp"
#include "../components/buildingComp.h"
#include "../components/rtsPlayerState.h"
#include "../utils/building_placement.h"
#include "../utils/building_info.h"
#include "../utils/rts_ui_draw.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <SFML/Graphics.hpp>

class BuildingPlacementRenderSystem : public ISystem {
public:
    BuildingPlacementRenderSystem(sf::RenderWindow& window, sf::RenderTarget& target,
        InputState& input, TileMap& map)
        : m_window(window), m_target(target), m_input(input), m_map(map) {
        priority = 11;
    }

    void onUpdate(Registry& registry, float) override {
        ECS::RtsHudState::PlacementMode mode = ECS::RtsHudState::PlacementMode::None;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            mode = hud.placementMode;
        });
        if (mode == ECS::RtsHudState::PlacementMode::None) return;

        const auto type = Rts::buildingTypeForPlacement(mode);
        const sf::Vector2f mouseWorld = m_window.mapPixelToCoords({
            static_cast<int>(m_input.mouseScreenPos.x),
            static_cast<int>(m_input.mouseScreenPos.y) });
        const sf::Vector2i tile = m_map.worldToTile(mouseWorld);
        const Rts::BuildingFootprint footprint = Rts::buildingFootprint(type);
        const sf::Vector2f topLeft = m_map.tileToWorld(tile.x, tile.y);
        const sf::Vector2f tileFootprintSize(
            footprint.tiles.x * static_cast<float>(m_map.tileSize),
            footprint.tiles.y * static_cast<float>(m_map.tileSize));

        bool valid = false;
        registry.view<ECS::PlayerEconomy>([&](Entity, ECS::PlayerEconomy& economy) {
            const int cost = Rts::buildingCost(type);
            valid = economy.gold >= cost && Rts::isValidBuildingPlacement(registry, m_map, type, tile);
        });
        const sf::Color tint = valid ? sf::Color(242, 126, 184, 90) : sf::Color(239, 94, 112, 100);
        const sf::Color edge = valid ? sf::Color(255, 188, 220, 230) : sf::Color(255, 142, 150, 240);

        RtsUI::drawRoundedRect(m_target,
            sf::FloatRect(topLeft, tileFootprintSize), tint, edge, 2.f, 8.f);

        sf::RectangleShape ghost(footprint.size);
        ghost.setPosition(Rts::buildingCenterForTile(m_map, tile, type) - footprint.size * 0.5f);
        ghost.setFillColor(sf::Color(tint.r, tint.g, tint.b, 125));
        ghost.setOutlineColor(edge);
        ghost.setOutlineThickness(2.f);
        m_target.draw(ghost);
    }

private:
    sf::RenderWindow& m_window;
    sf::RenderTarget& m_target;
    InputState& m_input;
    TileMap& m_map;
};
