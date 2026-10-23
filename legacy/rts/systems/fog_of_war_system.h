#pragma once

#include <SFML/Graphics.hpp>
#include "ecs/registry.h"
#include "ecs/system.h"
#include "components/buildingComp.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "../RtsTile_map.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

class FogOfWarSystem : public ISystem {
public:
    explicit FogOfWarSystem(const TileMap& map)
        : m_worldSize(static_cast<float>(map.width * map.tileSize),
            static_cast<float>(map.height * map.tileSize)) {
        priority = 6;
        const sf::Vector2u maskSize{
            std::max(1u, static_cast<unsigned int>((m_worldSize.x + kResolutionScale - 1.f) / kResolutionScale)),
            std::max(1u, static_cast<unsigned int>((m_worldSize.y + kResolutionScale - 1.f) / kResolutionScale))
        };
        const sf::View mapView(sf::FloatRect({ 0.f, 0.f }, m_worldSize));

        m_ready = m_visible.resize(maskSize)
            && m_explored[0].resize(maskSize)
            && m_explored[1].resize(maskSize);
        if (!m_ready) return;

        m_visible.setView(mapView);
        m_visible.setSmooth(true);
        for (sf::RenderTexture& explored : m_explored) {
            explored.setView(mapView);
            explored.setSmooth(true);
            explored.clear(sf::Color::Black);
            explored.display();
        }
        m_visible.clear(sf::Color::Black);
        m_visibilityWidth = std::max(1u, static_cast<unsigned int>(
            std::ceil(m_worldSize.x / kVisibilityCellSize)));
        m_visibilityHeight = std::max(1u, static_cast<unsigned int>(
            std::ceil(m_worldSize.y / kVisibilityCellSize)));
        m_currentVisibility.assign(static_cast<std::size_t>(m_visibilityWidth) * m_visibilityHeight, 0);
        m_exploredVisibility.assign(m_currentVisibility.size(), 0);
        m_visible.display();

        m_shaderReady = sf::Shader::isAvailable()
            && m_compositeShader.loadFromFile("games/rts/assets/fog_of_war.frag",
                sf::Shader::Type::Fragment);
    }

    void onUpdate(Registry& registry, float dt) override {
        if (!m_ready) return;
        m_refreshTimer += dt;
        if (m_hasVisibility && m_refreshTimer < kRefreshInterval) return;
        m_refreshTimer = 0.f;

        m_visionVertices.clear();
        std::fill(m_currentVisibility.begin(), m_currentVisibility.end(), 0);
        registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit,
            Transform2D& transform) {
            if (unit.team != ECS::Unit::Team::Player || unit.pv <= 0) return;
            const sf::Vector2f position = transform.getPosition();
            appendVision(position, unitVisionRadius(unit.type));
            rasterizeVision(position, unitVisionRadius(unit.type));
        });
        registry.view<ECS::Building, Transform2D>([&](Entity, ECS::Building& building,
            Transform2D& transform) {
            // la tour ne voit qu'une fois finie
            const float neededProgress = building.type == ECS::Building::Type::Tower ? 1.f : 0.2f;
            if (building.pv <= 0 || building.constructionProgress < neededProgress) return;
            const sf::Vector2f position = transform.getPosition();
            appendVision(position, buildingVisionRadius(building.type));
            rasterizeVision(position, buildingVisionRadius(building.type));
        });

        registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit,
            Transform2D& transform) {
            unit.visibleToPlayer = unit.team == ECS::Unit::Team::Player
                || (unit.pv > 0 && isVisible(transform.getPosition()));
        });

        m_visible.clear(sf::Color::Black);
        if (m_visionVertices.getVertexCount() > 0)
            m_visible.draw(m_visionVertices);
        m_visible.display();
        for (std::size_t i = 0; i < m_currentVisibility.size(); ++i) {
            if (m_currentVisibility[i]) m_exploredVisibility[i] = 1;
        }
        ++m_visibilityVersion;

        const std::size_t nextIndex = 1u - m_exploredIndex;
        sf::RenderTexture& nextExplored = m_explored[nextIndex];
        nextExplored.clear(sf::Color::Black);
        sf::Sprite oldExplored(m_explored[m_exploredIndex].getTexture());
        oldExplored.setScale({ m_worldSize.x / static_cast<float>(m_explored[m_exploredIndex].getSize().x),
            m_worldSize.y / static_cast<float>(m_explored[m_exploredIndex].getSize().y) });
        nextExplored.draw(oldExplored, sf::BlendNone);
        sf::Sprite currentVisibility(m_visible.getTexture());
        currentVisibility.setScale({ m_worldSize.x / static_cast<float>(m_visible.getSize().x),
            m_worldSize.y / static_cast<float>(m_visible.getSize().y) });
        nextExplored.draw(currentVisibility, sf::BlendMax);
        nextExplored.display();
        m_exploredIndex = nextIndex;
        m_hasVisibility = true;
    }

    const sf::Texture& visibleTexture() const { return m_visible.getTexture(); }
    const sf::Texture& exploredTexture() const { return m_explored[m_exploredIndex].getTexture(); }
    sf::Shader& compositeShader() { return m_compositeShader; }
    bool shaderReady() const { return m_shaderReady; }
    sf::Vector2f worldSize() const { return m_worldSize; }
    uint64_t visibilityVersion() const { return m_visibilityVersion; }

    bool isVisible(sf::Vector2f worldPosition) const {
        const std::size_t index = visibilityIndex(worldPosition);
        return index < m_currentVisibility.size() && m_currentVisibility[index] != 0;
    }

    bool isExplored(sf::Vector2f worldPosition) const {
        const std::size_t index = visibilityIndex(worldPosition);
        return index < m_exploredVisibility.size() && m_exploredVisibility[index] != 0;
    }

private:
    // Le masque reste lissé à l'affichage ; une résolution moitié moins grande
    // sur chaque axe réduit par quatre le travail de mise à jour du brouillard.
    static constexpr float kResolutionScale = 8.f;
    static constexpr float kRefreshInterval = 0.12f;
    static constexpr float kVisibilityCellSize = 16.f;
    static constexpr std::size_t kVisionSegments = 24;
    static constexpr float kSolidRadiusRatio = 0.82f;
    static constexpr float kVisionRangeMultiplier = 1.1f;

    sf::Vector2f m_worldSize;
    sf::RenderTexture m_visible;
    std::array<sf::RenderTexture, 2> m_explored;
    sf::Shader m_compositeShader;
    sf::VertexArray m_visionVertices{ sf::PrimitiveType::Triangles };
    std::size_t m_exploredIndex = 0;
    float m_refreshTimer = 0.f;
    bool m_ready = false;
    bool m_shaderReady = false;
    bool m_hasVisibility = false;
    unsigned int m_visibilityWidth = 0;
    unsigned int m_visibilityHeight = 0;
    uint64_t m_visibilityVersion = 0;
    std::vector<uint8_t> m_currentVisibility;
    std::vector<uint8_t> m_exploredVisibility;

    std::size_t visibilityIndex(sf::Vector2f position) const {
        if (position.x < 0.f || position.y < 0.f
            || position.x >= m_worldSize.x || position.y >= m_worldSize.y
            || m_visibilityWidth == 0 || m_visibilityHeight == 0)
            return m_currentVisibility.size();
        const unsigned int x = std::min(m_visibilityWidth - 1,
            static_cast<unsigned int>(position.x / kVisibilityCellSize));
        const unsigned int y = std::min(m_visibilityHeight - 1,
            static_cast<unsigned int>(position.y / kVisibilityCellSize));
        return static_cast<std::size_t>(y) * m_visibilityWidth + x;
    }

    void rasterizeVision(sf::Vector2f center, float radius) {
        if (m_visibilityWidth == 0 || m_visibilityHeight == 0) return;
        const int minX = std::max(0, static_cast<int>(std::floor((center.x - radius) / kVisibilityCellSize)));
        const int maxX = std::min(static_cast<int>(m_visibilityWidth) - 1,
            static_cast<int>(std::floor((center.x + radius) / kVisibilityCellSize)));
        const int minY = std::max(0, static_cast<int>(std::floor((center.y - radius) / kVisibilityCellSize)));
        const int maxY = std::min(static_cast<int>(m_visibilityHeight) - 1,
            static_cast<int>(std::floor((center.y + radius) / kVisibilityCellSize)));
        const float radiusSquared = radius * radius;
        for (int y = minY; y <= maxY; ++y) {
            const float dy = (static_cast<float>(y) + 0.5f) * kVisibilityCellSize - center.y;
            const float remainingX = radiusSquared - dy * dy;
            if (remainingX < 0.f) continue;
            const float halfWidth = std::sqrt(remainingX);
            const int rowMinX = std::max(minX, static_cast<int>(std::ceil(
                (center.x - halfWidth) / kVisibilityCellSize - 0.5f)));
            const int rowMaxX = std::min(maxX, static_cast<int>(std::floor(
                (center.x + halfWidth) / kVisibilityCellSize - 0.5f)));
            const std::size_t row = static_cast<std::size_t>(y) * m_visibilityWidth;
            for (int x = rowMinX; x <= rowMaxX; ++x)
                m_currentVisibility[row + static_cast<std::size_t>(x)] = 1;
        }
    }

    static float unitVisionRadius(ECS::Unit::Type type) {
        float radius = 210.f;
        switch (type) {
        case ECS::Unit::Type::Archer: radius = 270.f; break;
        case ECS::Unit::Type::Builder: radius = 155.f; break;
        case ECS::Unit::Type::Healer: radius = 210.f; break;
        case ECS::Unit::Type::Melee: radius = 225.f; break;
        case ECS::Unit::Type::Soldier:
        default: break;
        }
        return radius * kVisionRangeMultiplier;
    }

    static float buildingVisionRadius(ECS::Building::Type type) {
        float radius = 210.f;
        switch (type) {
        case ECS::Building::Type::Headquarters: radius = 360.f; break;
        case ECS::Building::Type::Barracks: radius = 260.f; break;
        case ECS::Building::Type::Farm: radius = 210.f; break;
        case ECS::Building::Type::Tower: radius = 520.f; break; // la tour voit plus loin que tout le reste
        }
        return radius * kVisionRangeMultiplier;
    }

    static const std::array<sf::Vector2f, kVisionSegments>& circleDirections() {
        static const std::array<sf::Vector2f, kVisionSegments> directions = [] {
            std::array<sf::Vector2f, kVisionSegments> result{};
            constexpr float twoPi = 6.28318530718f;
            for (std::size_t i = 0; i < kVisionSegments; ++i) {
                const float angle = twoPi * static_cast<float>(i) / static_cast<float>(kVisionSegments);
                result[i] = { std::cos(angle), std::sin(angle) };
            }
            return result;
        }();
        return directions;
    }

    void appendVision(sf::Vector2f center, float radius) {
        const auto& directions = circleDirections();
        const float innerRadius = radius * kSolidRadiusRatio;
        const sf::Color solid(255, 255, 255, 255);
        const sf::Color transparent(255, 255, 255, 0);
        for (std::size_t i = 0; i < kVisionSegments; ++i) {
            const std::size_t next = (i + 1) % kVisionSegments;
            const sf::Vector2f innerA = center + directions[i] * innerRadius;
            const sf::Vector2f innerB = center + directions[next] * innerRadius;
            const sf::Vector2f outerA = center + directions[i] * radius;
            const sf::Vector2f outerB = center + directions[next] * radius;

            m_visionVertices.append(sf::Vertex{ center, solid });
            m_visionVertices.append(sf::Vertex{ innerA, solid });
            m_visionVertices.append(sf::Vertex{ innerB, solid });
            m_visionVertices.append(sf::Vertex{ innerA, solid });
            m_visionVertices.append(sf::Vertex{ outerA, transparent });
            m_visionVertices.append(sf::Vertex{ outerB, transparent });
            m_visionVertices.append(sf::Vertex{ innerA, solid });
            m_visionVertices.append(sf::Vertex{ outerB, transparent });
            m_visionVertices.append(sf::Vertex{ innerB, solid });
        }
    }
};
