#pragma once
#include "utils/resource_loader.h"
#include "RtsTile_map.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

// Génère des feuilles de tiles "dual-grid" sans aucun fichier image, au format attendu
// par TileMapRenderSystem : 16 colonnes (une par masque de coins, 0..15) x N lignes
// (variantes, ou frames si le type est animé), chaque case faisant cellSize pixels.
//
// Bits du masque (voir TileMap::vertexPieces) : 8 = haut-gauche, 4 = haut-droite,
// 2 = bas-gauche, 1 = bas-droite.
class TilesetFactory {
public:
    enum class Pattern { Noise, Grass, Water, Sand, Bricks, Slabs };

    struct Style {
        sf::Color color;
        Pattern pattern = Pattern::Noise;
        uint32_t seed = 1;
    };

    static sf::Image createDualGridSheet(const Style& style, uint32_t cell = 16, uint32_t rows = 4) {
        sf::Image image({ cell * 16, cell * rows }, sf::Color::Transparent);
        const float q = cell / 4.f;      // centre d'un quart de case
        const float span = cell / 2.f;   // distance entre deux centres de quart

        for (uint32_t row = 0; row < rows; ++row) {
            for (uint32_t mask = 1; mask < 16; ++mask) {
                const float tl = (mask & 8) ? 1.f : 0.f, tr = (mask & 4) ? 1.f : 0.f;
                const float bl = (mask & 2) ? 1.f : 0.f, br = (mask & 1) ? 1.f : 0.f;

                for (uint32_t y = 0; y < cell; ++y) {
                    for (uint32_t x = 0; x < cell; ++x) {
                        // Forme arrondie : interpolation bilinéaire des 4 coins
                        const float u = std::clamp((x + 0.5f - q) / span, 0.f, 1.f);
                        const float v = std::clamp((y + 0.5f - q) / span, 0.f, 1.f);
                        const float val = (tl * (1 - u) + tr * u) * (1 - v) + (bl * (1 - u) + br * u) * v;

                        // Bord irrégulier, mais nul sur les bords de case pour que les pièces se raccordent
                        const float edgeDist = (float)std::min({ x, y, cell - 1 - x, cell - 1 - y }) / (cell / 2.f);
                        const float wobble = (rand01(x, y, row * 31 + mask, style.seed) - 0.5f) * 0.18f * edgeDist;
                        const float threshold = 0.5f + wobble;
                        if (val <= threshold) continue;

                        sf::Color c = patternColor(style, x, y, row, cell);
                        if (val < threshold + 0.09f && mask != 15) c = shade(c, 0.72f);   // liseré
                        image.setPixel({ mask * cell + x, row * cell + y }, c);
                    }
                }
            }
        }
        return image;
    }

    // Couleurs/motifs par défaut des terrains de TileMap
    static Style defaultStyle(TileType type) {
        switch (type) {
        case TileType::Sand:        return { sf::Color(222, 196, 140), Pattern::Sand,   11 };
        case TileType::Water:       return { sf::Color(52, 110, 170),  Pattern::Water,  23 };
        case TileType::Grass:       return { sf::Color(86, 150, 72),   Pattern::Grass,  37 };
        case TileType::DirtPath:    return { sf::Color(140, 100, 64),  Pattern::Noise,  41 };
        case TileType::BrickPath:   return { sf::Color(156, 84, 70),   Pattern::Bricks, 53 };
        case TileType::DonjonFloor: return { sf::Color(110, 110, 120), Pattern::Slabs,  67 };
        default:                    return { sf::Color(200, 0, 200),   Pattern::Noise,  71 };
        }
    }

private:
    static uint32_t hash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
        uint32_t h = x * 374761393u + y * 668265263u + z * 2246822519u + seed * 3266489917u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }
    static float rand01(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
        return (hash(x, y, z, seed) & 0xFFFF) / 65535.f;
    }
    static sf::Color shade(sf::Color c, float f) {
        auto ch = [f](uint8_t v) { return (uint8_t)std::clamp(v * f, 0.f, 255.f); };
        return { ch(c.r), ch(c.g), ch(c.b), c.a };
    }

    // Motifs périodiques sur la taille de case => raccords invisibles entre pièces
    static sf::Color patternColor(const Style& s, uint32_t x, uint32_t y, uint32_t row, uint32_t cell) {
        const float n = rand01(x, y, row, s.seed);
        float f = 0.93f + n * 0.14f;   // grain de base

        switch (s.pattern) {
        case Pattern::Grass:
            if (n > 0.9f) f = 1.18f;           // brins clairs
            else if (n < 0.08f) f = 0.8f;      // touffes sombres
            break;
        case Pattern::Sand:
            if (n > 0.94f) f = 0.82f;          // petits cailloux
            break;
        case Pattern::Water: {
            // vaguelettes qui se déplacent d'une frame (ligne) à l'autre
            const float phase = (x + y * 0.5f + row * (cell / 4.f)) * (6.2831853f / cell);
            const float wave = std::sin(phase) * std::sin((y + row) * (6.2831853f / (cell / 2.f)));
            f = 0.96f + n * 0.06f + (wave > 0.75f ? 0.22f : 0.f);
            break;
        }
        case Pattern::Bricks: {
            const uint32_t bh = cell / 4, bw = cell / 2;
            const uint32_t line = y / bh;
            const uint32_t off = (line % 2) ? bw / 2 : 0;
            if (y % bh == 0 || (x + off) % bw == 0) f = 0.62f;   // joints
            break;
        }
        case Pattern::Slabs: {
            const uint32_t sz = cell / 2;
            if (x % sz == 0 || y % sz == 0) f = 0.7f;
            else if (x % sz == 1 || y % sz == 1) f = 1.1f;      // arête éclairée
            break;
        }
        case Pattern::Noise:
        default:
            if (n > 0.95f) f = 0.78f;
            break;
        }
        return shade(s.color, f);
    }
};

// Loader de textures en mémoire : on y range les feuilles générées, et
// TileMapRenderSystem les retrouve par leur identifiant comme un fichier.
class MemoryTextureLoader : public IResourceLoader {
public:
    void add(const std::string& id, const sf::Image& image) {
        sf::Texture tex;
        if (tex.loadFromImage(image)) m_textures[id] = std::move(tex);
    }

    sf::Texture& loadTexture(const std::string& id) override {
        auto it = m_textures.find(id);
        if (it != m_textures.end()) return it->second;
        // inconnu : damier magenta (comme LooseFileLoader)
        sf::Image img({ 16, 16 }, sf::Color::Magenta);
        for (uint32_t y = 0; y < 16; ++y)
            for (uint32_t x = 0; x < 16; ++x)
                if (((x / 4) + (y / 4)) % 2) img.setPixel({ x, y }, sf::Color::Black);
        add(id, img);
        return m_textures[id];
    }

    void unloadTexture(const std::string& id) override { m_textures.erase(id); }

private:
    std::unordered_map<std::string, sf::Texture> m_textures;
};
