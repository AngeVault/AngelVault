#pragma once

#include "../components/buildingComp.h"
#include <SFML/System/Vector2.hpp>
#include <array>
#include <fstream>
#include <optional>
#include <string>

// une texture png par batiment : games/rts/assets/game/materials/buildings/<nom>.png
// le batiment prend la taille du png, sans png il garde son carre de couleur
namespace Rts {

    inline const char* buildingTextureName(ECS::Building::Type type) {
        switch (type) {
        case ECS::Building::Type::Headquarters: return "headquarters";
        case ECS::Building::Type::Farm: return "farm";
        case ECS::Building::Type::Barracks: return "barracks";
        case ECS::Building::Type::Tower: return "tower";
        }
        return "building";
    }

    inline std::string buildingTexturePath(ECS::Building::Type type) {
        return std::string("games/rts/assets/game/materials/buildings/") + buildingTextureName(type) + ".png";
    }

    // lit la taille dans l'en-tete du png (pas besoin de charger l'image)
    inline std::optional<sf::Vector2i> readPngSize(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        unsigned char header[24];
        if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) return std::nullopt;
        static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        for (int i = 0; i < 8; ++i)
            if (header[i] != signature[i]) return std::nullopt;
        if (header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R') return std::nullopt;
        const auto readInt = [&](int offset) {
            return (static_cast<int>(header[offset]) << 24) | (static_cast<int>(header[offset + 1]) << 16)
                | (static_cast<int>(header[offset + 2]) << 8) | static_cast<int>(header[offset + 3]);
        };
        const int width = readInt(16);
        const int height = readInt(20);
        if (width < 8 || height < 8 || width > 2048 || height > 2048) return std::nullopt;
        return sf::Vector2i(width, height);
    }

    // les tailles sont gardees en cache, reloadBuildingTextures() les relit (appele au lancement d'une partie)
    struct BuildingTextureCache {
        std::array<std::optional<sf::Vector2i>, 4> sizes;
        std::array<bool, 4> loaded{};
    };

    inline BuildingTextureCache& buildingTextureCache() {
        static BuildingTextureCache cache;
        return cache;
    }

    inline void reloadBuildingTextures() {
        buildingTextureCache() = BuildingTextureCache{};
    }

    // taille du png du batiment, rien si le fichier n'existe pas
    inline std::optional<sf::Vector2i> buildingTextureSize(ECS::Building::Type type) {
        BuildingTextureCache& cache = buildingTextureCache();
        const std::size_t index = static_cast<std::size_t>(type);
        if (!cache.loaded[index]) {
            cache.sizes[index] = readPngSize(buildingTexturePath(type));
            cache.loaded[index] = true;
        }
        return cache.sizes[index];
    }

    // id de texture pour le loader (le chemin), vide s'il n'y a pas de png
    inline std::string buildingSpriteId(ECS::Building::Type type) {
        return buildingTextureSize(type) ? buildingTexturePath(type) : std::string();
    }

}
