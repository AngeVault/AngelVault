#pragma once

#include "../RtsTile_map.hpp"
#include "../components/buildingComp.h"
#include "../components/camera2d.h"
#include "../components/unitComp.h"
#include "../utils/rts_ui_draw.h"
#include "fog_of_war_system.h"
#include <components/transform2d.h>
#include "ecs/registry.h"
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>

class MinimapRenderSystem : public ISystem {
public:
    MinimapRenderSystem(sf::RenderWindow& window, TileMap& map, Camera2d& camera,
        FogOfWarSystem& fog)
        : m_window(window), m_map(map), m_camera(camera), m_fog(fog) {
        priority = 95;
    }

    void onUpdate(Registry& registry, float) override {
        const sf::Vector2u windowSize = m_window.getSize();
        const sf::FloatRect panelBounds = getPanelBounds(windowSize, m_map);
        if (panelBounds.size.x < 80.f || m_map.width == 0 || m_map.height == 0) return;

        const sf::View previousView = m_window.getView();
        m_window.setView(sf::View(sf::FloatRect({ 0.f, 0.f },
            { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) })));

        RtsUI::drawRoundedRect(m_window, panelBounds, sf::Color(255, 238, 246, 245),
            sf::Color(235, 166, 197), 1.5f, 14.f);

        const sf::FloatRect mapBounds = getMapBounds(windowSize, m_map);
        const sf::Vector2f mapPosition = mapBounds.position;
        const sf::Vector2f mapSize = mapBounds.size;

        sf::RectangleShape ground(mapSize);
        ground.setPosition(mapPosition);
        ground.setFillColor(sf::Color(91, 137, 77));
        m_window.draw(ground);

        if (m_terrainVersion != m_map.version
            || m_cachedMapBounds.position != mapBounds.position
            || m_cachedMapBounds.size != mapBounds.size) {
            rebuildTerrain(mapBounds);
        }
        m_window.draw(m_terrain);

        if (m_fogVersion != m_fog.visibilityVersion()
            || m_cachedFogBounds.position != mapBounds.position
            || m_cachedFogBounds.size != mapBounds.size)
            rebuildFog(mapBounds);
        m_window.draw(m_fogOverlay);

        for (auto& markers : m_buildingMarkers) markers.clear();
        m_playerMarkers.clear();
        m_enemyMarkers.clear();
        registry.view<ECS::Building, Transform2D>([&](Entity, ECS::Building& building, Transform2D& transform) {
            appendSquare(m_buildingMarkers[static_cast<size_t>(building.type)],
                toMinimap(transform.getPosition(), mapBounds), 5.f, buildingColor(building.type));
        });
        registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit, Transform2D& transform) {
            if (unit.team == ECS::Unit::Team::Enemy && !unit.visibleToPlayer) return;
            sf::VertexArray& markers = unit.team == ECS::Unit::Team::Enemy
                ? m_enemyMarkers : m_playerMarkers;
            appendCircle(markers, toMinimap(transform.getPosition(), mapBounds), 2.2f,
                unit.team == ECS::Unit::Team::Enemy
                    ? sf::Color(222, 70, 83) : sf::Color(255, 239, 248));
        });
        for (const auto& markers : m_buildingMarkers) m_window.draw(markers);
        m_window.draw(m_playerMarkers);
        m_window.draw(m_enemyMarkers);

        const float worldWidth = static_cast<float>(m_map.width * m_map.tileSize);
        const float worldHeight = static_cast<float>(m_map.height * m_map.tileSize);
        const float viewWidth = static_cast<float>(windowSize.x) / m_camera.zoom;
        const float viewHeight = static_cast<float>(windowSize.y) / m_camera.zoom;
        const float left = std::max(0.f, m_camera.position.x - viewWidth * 0.5f);
        const float top = std::max(0.f, m_camera.position.y - viewHeight * 0.5f);
        const float right = std::min(worldWidth, m_camera.position.x + viewWidth * 0.5f);
        const float bottom = std::min(worldHeight, m_camera.position.y + viewHeight * 0.5f);
        if (right > left && bottom > top) {
            sf::RectangleShape cameraRect({ (right - left) / worldWidth * mapSize.x,
                (bottom - top) / worldHeight * mapSize.y });
            cameraRect.setPosition(mapPosition + sf::Vector2f(left / worldWidth * mapSize.x,
                top / worldHeight * mapSize.y));
            cameraRect.setFillColor(sf::Color::Transparent);
            cameraRect.setOutlineColor(sf::Color(255, 255, 255, 235));
            cameraRect.setOutlineThickness(1.f);
            m_window.draw(cameraRect);
        }

        // Redessine le contour par-dessus la carte pour garder un cadre net.
        RtsUI::drawRoundedRect(m_window, panelBounds, sf::Color::Transparent,
            sf::Color(235, 151, 188), 2.f, 14.f);

        m_window.setView(previousView);
    }

private:
    sf::RenderWindow& m_window;
    TileMap& m_map;
    Camera2d& m_camera;
    FogOfWarSystem& m_fog;
    sf::VertexArray m_terrain{ sf::PrimitiveType::Triangles };
    sf::VertexArray m_fogOverlay{ sf::PrimitiveType::Triangles };
    std::array<sf::VertexArray, 3> m_buildingMarkers{
        sf::VertexArray(sf::PrimitiveType::Triangles),
        sf::VertexArray(sf::PrimitiveType::Triangles),
        sf::VertexArray(sf::PrimitiveType::Triangles) };
    sf::VertexArray m_playerMarkers{ sf::PrimitiveType::Triangles };
    sf::VertexArray m_enemyMarkers{ sf::PrimitiveType::Triangles };
    uint32_t m_terrainVersion = UINT32_MAX;
    sf::FloatRect m_cachedMapBounds;
    sf::FloatRect m_cachedFogBounds;
    uint64_t m_fogVersion = UINT64_MAX;
    // La minimap est petite : une case de rendu résume 4x4 tiles du monde.
    static constexpr uint32_t kMinimapTileStride = 4;

    void rebuildTerrain(const sf::FloatRect& bounds) {
        m_terrain.clear();
        m_terrain.setPrimitiveType(sf::PrimitiveType::Triangles);
        for (uint32_t y = 0; y < m_map.height; y += kMinimapTileStride) {
            const uint32_t cellHeight = std::min(kMinimapTileStride, m_map.height - y);
            for (uint32_t x = 0; x < m_map.width; x += kMinimapTileStride) {
                const uint32_t cellWidth = std::min(kMinimapTileStride, m_map.width - x);
                unsigned int red = 0, green = 0, blue = 0, alpha = 0, samples = 0;
                for (uint32_t sampleY = y; sampleY < y + cellHeight; ++sampleY) {
                    for (uint32_t sampleX = x; sampleX < x + cellWidth; ++sampleX) {
                        const sf::Color sampleColor = tileColor(m_map.getType(
                            static_cast<int>(sampleX), static_cast<int>(sampleY)));
                        if (sampleColor.a == 0) continue;
                        red += sampleColor.r;
                        green += sampleColor.g;
                        blue += sampleColor.b;
                        alpha += sampleColor.a;
                        ++samples;
                    }
                }
                if (samples == 0) continue;
                const sf::Color color(static_cast<uint8_t>(red / samples),
                    static_cast<uint8_t>(green / samples),
                    static_cast<uint8_t>(blue / samples),
                    static_cast<uint8_t>(alpha / samples));
                if (color.a == 0) continue;
                const float left = bounds.position.x
                    + static_cast<float>(x) / static_cast<float>(m_map.width) * bounds.size.x;
                const float top = bounds.position.y
                    + static_cast<float>(y) / static_cast<float>(m_map.height) * bounds.size.y;
                const float right = bounds.position.x
                    + static_cast<float>(x + cellWidth) / static_cast<float>(m_map.width) * bounds.size.x;
                const float bottom = bounds.position.y
                    + static_cast<float>(y + cellHeight) / static_cast<float>(m_map.height) * bounds.size.y;
                const sf::Vector2f topLeft{ left, top };
                const sf::Vector2f topRight{ right, top };
                const sf::Vector2f bottomRight{ right, bottom };
                const sf::Vector2f bottomLeft{ left, bottom };
                m_terrain.append(sf::Vertex{ topLeft, color });
                m_terrain.append(sf::Vertex{ topRight, color });
                m_terrain.append(sf::Vertex{ bottomRight, color });
                m_terrain.append(sf::Vertex{ topLeft, color });
                m_terrain.append(sf::Vertex{ bottomRight, color });
                m_terrain.append(sf::Vertex{ bottomLeft, color });
            }
        }
        m_terrainVersion = m_map.version;
        m_cachedMapBounds = bounds;
    }

    void rebuildFog(const sf::FloatRect& bounds) {
        m_fogOverlay.clear();
        m_fogOverlay.setPrimitiveType(sf::PrimitiveType::Triangles);
        const float tileSize = static_cast<float>(m_map.tileSize);
        for (uint32_t y = 0; y < m_map.height; y += kMinimapTileStride) {
            const uint32_t cellHeight = std::min(kMinimapTileStride, m_map.height - y);
            const uint32_t sampleY = y + cellHeight / 2;
            for (uint32_t x = 0; x < m_map.width; x += kMinimapTileStride) {
                const uint32_t cellWidth = std::min(kMinimapTileStride, m_map.width - x);
                const uint32_t sampleX = x + cellWidth / 2;
                const sf::Vector2f sample{ (static_cast<float>(sampleX) + 0.5f) * tileSize,
                    (static_cast<float>(sampleY) + 0.5f) * tileSize };
                if (m_fog.isVisible(sample)) continue;
                const sf::Color shade = m_fog.isExplored(sample)
                    ? sf::Color(35, 27, 38, 165) : sf::Color(20, 16, 26, 235);
                const float left = bounds.position.x
                    + static_cast<float>(x) / static_cast<float>(m_map.width) * bounds.size.x;
                const float top = bounds.position.y
                    + static_cast<float>(y) / static_cast<float>(m_map.height) * bounds.size.y;
                const float right = bounds.position.x
                    + static_cast<float>(x + cellWidth) / static_cast<float>(m_map.width) * bounds.size.x;
                const float bottom = bounds.position.y
                    + static_cast<float>(y + cellHeight) / static_cast<float>(m_map.height) * bounds.size.y;
                const sf::Vector2f topLeft{ left, top };
                const sf::Vector2f topRight{ right, top };
                const sf::Vector2f bottomRight{ right, bottom };
                const sf::Vector2f bottomLeft{ left, bottom };
                m_fogOverlay.append(sf::Vertex{ topLeft, shade });
                m_fogOverlay.append(sf::Vertex{ topRight, shade });
                m_fogOverlay.append(sf::Vertex{ bottomRight, shade });
                m_fogOverlay.append(sf::Vertex{ topLeft, shade });
                m_fogOverlay.append(sf::Vertex{ bottomRight, shade });
                m_fogOverlay.append(sf::Vertex{ bottomLeft, shade });
            }
        }
        m_cachedFogBounds = bounds;
        m_fogVersion = m_fog.visibilityVersion();
    }

public:
    static sf::FloatRect getPanelBounds(sf::Vector2u windowSize, const TileMap& map) {
        constexpr float margin = 14.f;
        constexpr float padding = 8.f;
        if (map.width == 0 || map.height == 0) return {};

        const float maxWidth = std::min(172.f,
            static_cast<float>(windowSize.x) - margin * 2.f);
        const float maxHeight = static_cast<float>(windowSize.y) - margin * 2.f;
        const float aspect = static_cast<float>(map.width) / static_cast<float>(map.height);
        float mapWidth = std::max(0.f, maxWidth - padding * 2.f);
        float mapHeight = mapWidth / aspect;
        if (mapHeight + padding * 2.f > maxHeight) {
            mapHeight = std::max(0.f, maxHeight - padding * 2.f);
            mapWidth = mapHeight * aspect;
        }

        const sf::Vector2f panelSize{ mapWidth + padding * 2.f, mapHeight + padding * 2.f };
        if (panelSize.x < 80.f || panelSize.y < 80.f) return {};
        return sf::FloatRect({ static_cast<float>(windowSize.x) - panelSize.x - margin,
            static_cast<float>(windowSize.y) - panelSize.y - margin }, panelSize);
    }

    static sf::FloatRect getMapBounds(sf::Vector2u windowSize, const TileMap& map) {
        constexpr float padding = 8.f;
        const sf::FloatRect panel = getPanelBounds(windowSize, map);
        const sf::Vector2f innerSize = panel.size - sf::Vector2f(padding * 2.f, padding * 2.f);
        if (innerSize.x <= 0.f || innerSize.y <= 0.f || map.width == 0 || map.height == 0) return {};
        const float aspect = static_cast<float>(map.width) / static_cast<float>(map.height);
        sf::Vector2f mapSize = innerSize;
        if (mapSize.x / mapSize.y > aspect) mapSize.x = mapSize.y * aspect;
        else mapSize.y = mapSize.x / aspect;
        return sf::FloatRect(panel.position
            + sf::Vector2f((panel.size.x - mapSize.x) * 0.5f,
                (panel.size.y - mapSize.y) * 0.5f), mapSize);
    }

    static sf::Vector2f worldFromScreen(sf::Vector2f screen, sf::Vector2u windowSize,
        const TileMap& map) {
        const sf::FloatRect bounds = getMapBounds(windowSize, map);
        if (bounds.size.x <= 0.f || bounds.size.y <= 0.f) return {};
        const float x = std::clamp((screen.x - bounds.position.x) / bounds.size.x, 0.f, 1.f);
        const float y = std::clamp((screen.y - bounds.position.y) / bounds.size.y, 0.f, 1.f);
        return { x * static_cast<float>(map.width * map.tileSize),
            y * static_cast<float>(map.height * map.tileSize) };
    }

private:

    sf::Vector2f toMinimap(sf::Vector2f world, const sf::FloatRect& bounds) const {
        const float worldWidth = static_cast<float>(m_map.width * m_map.tileSize);
        const float worldHeight = static_cast<float>(m_map.height * m_map.tileSize);
        return bounds.position + sf::Vector2f(world.x / worldWidth * bounds.size.x,
            world.y / worldHeight * bounds.size.y);
    }

    static void appendSquare(sf::VertexArray& vertices, sf::Vector2f center,
        float size, sf::Color color) {
        const float half = size * 0.5f;
        const sf::Vector2f topLeft{ center.x - half, center.y - half };
        const sf::Vector2f topRight{ center.x + half, center.y - half };
        const sf::Vector2f bottomRight{ center.x + half, center.y + half };
        const sf::Vector2f bottomLeft{ center.x - half, center.y + half };
        vertices.append(sf::Vertex{ topLeft, color });
        vertices.append(sf::Vertex{ topRight, color });
        vertices.append(sf::Vertex{ bottomRight, color });
        vertices.append(sf::Vertex{ topLeft, color });
        vertices.append(sf::Vertex{ bottomRight, color });
        vertices.append(sf::Vertex{ bottomLeft, color });
    }

    static void appendCircle(sf::VertexArray& vertices, sf::Vector2f center,
        float radius, sf::Color color) {
        const auto& directions = circleDirections();
        for (size_t i = 0; i < directions.size(); ++i) {
            vertices.append(sf::Vertex{ center, color });
            vertices.append(sf::Vertex{ center + directions[i] * radius, color });
            vertices.append(sf::Vertex{ center + directions[(i + 1) % directions.size()] * radius, color });
        }
    }

    static const std::array<sf::Vector2f, 8>& circleDirections() {
        static const std::array<sf::Vector2f, 8> directions = [] {
            std::array<sf::Vector2f, 8> result{};
            constexpr float twoPi = 6.28318530718f;
            for (size_t i = 0; i < result.size(); ++i) {
                const float angle = twoPi * static_cast<float>(i) / static_cast<float>(result.size());
                result[i] = { std::cos(angle), std::sin(angle) };
            }
            return result;
        }();
        return directions;
    }

    static sf::Color tileColor(TileType type) {
        switch (type) {
        case TileType::Water: return sf::Color(90, 165, 207);
        case TileType::Sand: return sf::Color(218, 194, 132);
        case TileType::DirtPath: return sf::Color(154, 115, 79);
        case TileType::BrickPath: return sf::Color(163, 105, 93);
        case TileType::DonjonFloor: return sf::Color(125, 120, 132);
        case TileType::Wall: return sf::Color(76, 73, 82);
        default: return sf::Color::Transparent;
        }
    }

    static sf::Color buildingColor(ECS::Building::Type type) {
        switch (type) {
        case ECS::Building::Type::Headquarters: return sf::Color(255, 215, 97);
        case ECS::Building::Type::Farm: return sf::Color(137, 207, 112);
        case ECS::Building::Type::Barracks: return sf::Color(220, 131, 116);
        case ECS::Building::Type::Tower: return sf::Color(150, 170, 225);
        }
        return sf::Color::White;
    }
};
