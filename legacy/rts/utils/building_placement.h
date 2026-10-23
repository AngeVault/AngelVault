#pragma once

#include "../RtsTile_map.hpp"
#include "../components/buildingComp.h"
#include "building_texture.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <algorithm>
#include <cmath>

namespace Rts {
    struct BuildingFootprint {
        sf::Vector2i tiles;
        sf::Vector2f size;
    };

    inline BuildingFootprint buildingFootprint(ECS::Building::Type type) {
        // avec un png : la taille du png, et assez de cases pour la couvrir
        if (const auto texture = buildingTextureSize(type)) {
            const sf::Vector2f size(static_cast<float>(texture->x), static_cast<float>(texture->y));
            return { { std::max(2, static_cast<int>(std::ceil(size.x / 32.f))),
                       std::max(2, static_cast<int>(std::ceil(size.y / 32.f))) }, size };
        }
        if (type == ECS::Building::Type::Barracks)
            return { { 4, 3 }, { 100.f, 84.f } };
        if (type == ECS::Building::Type::Tower)
            return { { 2, 2 }, { 60.f, 76.f } };
        return { { 3, 3 }, { 92.f, 68.f } };
    }

    inline sf::Vector2f buildingCenterForTile(const TileMap& map, sf::Vector2i tile,
        ECS::Building::Type type) {
        const BuildingFootprint footprint = buildingFootprint(type);
        const sf::Vector2f topLeft = map.tileToWorld(tile.x, tile.y);
        return topLeft + sf::Vector2f(
            footprint.tiles.x * static_cast<float>(map.tileSize) * 0.5f,
            footprint.tiles.y * static_cast<float>(map.tileSize) * 0.5f);
    }

    inline bool isValidBuildingPlacement(Registry& registry, const TileMap& map,
        ECS::Building::Type type, sf::Vector2i tile) {
        const BuildingFootprint footprint = buildingFootprint(type);
        if (tile.x < 0 || tile.y < 0
            || tile.x + footprint.tiles.x > static_cast<int>(map.width)
            || tile.y + footprint.tiles.y > static_cast<int>(map.height))
            return false;

        for (int y = tile.y; y < tile.y + footprint.tiles.y; ++y)
            for (int x = tile.x; x < tile.x + footprint.tiles.x; ++x)
                if (map.getType(x, y) == TileType::Water)
                    return false;

        const sf::Vector2f center = buildingCenterForTile(map, tile, type);
        bool occupied = false;
        const float newHalf = std::max(footprint.size.x, footprint.size.y) * 0.5f;
        registry.view<ECS::Building, Transform2D>([&](Entity, ECS::Building& other, Transform2D& transform) {
            // 82 au minimum, plus si les batiments sont grands (leurs png)
            const float needed = std::max(82.f, newHalf + std::max(other.halfSize.x, other.halfSize.y) - 14.f);
            const sf::Vector2f position = transform.getPosition();
            if (std::hypot(position.x - center.x, position.y - center.y) < needed)
                occupied = true;
        });
        return !occupied;
    }
}
