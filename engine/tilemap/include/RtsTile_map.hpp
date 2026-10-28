#pragma once
#include "Layer.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <initializer_list>
#include <cmath>
#include <cstdint>

enum class TileType : uint8_t {
    None = 0,
    Sand,
    Water,
    Grass,
    BrickPath,
    DirtPath,
    DonjonFloor,
    Wall,
    Path,
    Count
};

// Flags couche logique (value)
constexpr uint8_t TF_SOLID = 0b10000000;
constexpr uint8_t TF_TYPE_VALUE = 0b01111111;

struct TileAnimation {
    float time = 0.f;
    float frameTime = 0.5f;
    int frameCount = 1;      // = nombre de LIGNES de la texture pour un type animé
    int currentFrame = 0;
    int variantCount = 1;
    int framesX = 1;
};

struct TileSet {
    std::unordered_map<int, std::vector<std::string>> variants;
    std::unordered_map<int, TileAnimation> animations;                
    std::unordered_map<std::string, TileAnimation> textureAnimations;  
    int cellSize = 16;  
};

// Une pièce visuelle posée sur un sommet (dual-grid)
struct VertexPiece {
    TileType type;
    uint8_t  mask;   // 1..15 : quels coins (tiles voisines) sont couverts par ce type
};

class TileMap {
public:
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t nbTiles = 0;
    uint32_t tileSize = 16;   // taille écran d'une tile ET d'une pièce visuelle

    // couches
    std::vector<uint8_t>  value;   // logique + solidité, stride = width + 1
    std::vector<uint16_t> walls;   // nbTiles — couche murs (sprites 32×32)

    TileSet tileset;

    uint32_t version = 0;        // la carte a changé
    uint32_t animVersion = 0;    // une frame d'animation a changé
    uint32_t layerVersion = 0;   // l'ordre des couches a changé

    // Ordre d'empilement des terrains, du DESSOUS vers le DESSUS.
    std::vector<TileType> layerOrder;

    // Eau tout en haut : son écume se pose par-dessus le sable, l'herbe et les chemins.
    TileMap(Layer* layer):m_layer(layer) {
        setLayerOrder({ TileType::Sand, TileType::Water,TileType::Grass, TileType::DirtPath,
                       TileType::BrickPath, TileType::DonjonFloor });
    }

    void setLayerOrder(std::initializer_list<TileType> order);

    void create(uint32_t w, uint32_t h);
    void setTile(int x, int y, TileType type);
    void fillRect(int x, int y, int w, int h, TileType type);

    int vertexPieces(int vx, int vy, VertexPiece out[4]) const;

    bool update(float dt);

    // Si la carte a grandi vers x<0 / y<0, tout a été décalé : récupère le décalage
    sf::Vector2i consumeGrowOffset() {
        auto o = pendingShift; pendingShift = { 0, 0 }; return o;
    }

    TileType getType(int x, int y) const {
        if (!inBounds(x, y)) return TileType::None;
        return (TileType)(value[vIdx(x, y)] & TF_TYPE_VALUE);
    }
    bool isSolid(int x, int y) const {
        if (!inBounds(x, y)) return true;
        return (value[vIdx(x, y)] & TF_SOLID) != 0;
    }

    sf::Vector2f tileToWorld(int x, int y) const {
        return { x * (float)tileSize, y * (float)tileSize };
    }
    sf::Vector2i worldToTile(sf::Vector2f w) const {
        return { (int)std::floor(w.x / tileSize), (int)std::floor(w.y / tileSize) };
    }

private:
    sf::Vector2i pendingShift{ 0, 0 };
    std::array<uint8_t, 256> layerRank{};   // 0 = pas un terrain dual-grid
    Layer* m_layer; // observed

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < (int)width && y < (int)height;
    }
    size_t vIdx(int x, int y) const { return (size_t)x + (size_t)y * (width + 1); }

    void writeTile(int x, int y, TileType type);
    sf::Vector2i grow(int newX, int newY);
    void ensureInBounds(int& x, int& y, sf::Vector2i* appliedShift = nullptr);
};