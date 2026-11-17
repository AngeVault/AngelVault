#pragma once
#include "tilemap/tileset.h"

#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ecs/registry.h"

enum class TileMapOrientation {
    Orthogonal,  
    Isometric,   
};

using TileCell = uint32_t;
inline constexpr uint16_t kAutoTile = 0xFFFF;
inline constexpr TileCell makeCell(int slot, int tile) {
    return slot < 0 ? 0u : (TileCell(slot + 1) << 16) | TileCell(uint16_t(tile));
}
inline constexpr int cellSlot(TileCell c) { return int(c >> 16) - 1; }     
inline constexpr int cellTile(TileCell c) { return int(c & 0xFFFF); }
inline constexpr bool cellIsAuto(TileCell c) { return c != 0 && (c & 0xFFFF) == kAutoTile; }

// où le layer se dessine par rapport aux entités
enum class TileLayerMode {
    Below,   // sous les entités, dans l'ordre des layers
    YSort,   // mélangé aux entités selon le y, comme un tileset ySort
    Above,   // par-dessus les entités
};

struct TileLayer {
    std::string name;
    TileLayerMode mode = TileLayerMode::Below;
    bool hidden = false;   // caché dans l'éditeur seulement, pas enregistré
    std::vector<TileCell> cells;
};

// une grille partagée (orientation, taille, tilesets) et ses layers, du dessous vers le dessus
class TileMapData {
public:
    TileMapOrientation orientation = TileMapOrientation::Orthogonal;
    sf::Vector2f tileSize{ 32.f, 32.f };  

    std::vector<std::shared_ptr<Tileset>> tilesets;

    std::string baseDir;

    int width() const { return m_width; }
    int height() const { return m_height; }
    void resize(int width, int height);     // garde le contenu (coin haut-gauche)
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }

    int layerCount() const { return int(m_layers.size()); }
    bool validLayer(int layer) const { return layer >= 0 && layer < int(m_layers.size()); }
    const TileLayer* layer(int layer) const { return validLayer(layer) ? &m_layers[std::size_t(layer)] : nullptr; }
    TileLayer* layer(int layer) { return validLayer(layer) ? &m_layers[std::size_t(layer)] : nullptr; }
    int addLayer(const std::string& name, TileLayerMode mode = TileLayerMode::Below, int index = -1);   // -1 = au-dessus
    void removeLayer(int layer);
    void moveLayer(int from, int to);

    TileCell get(int layer, int x, int y) const;
    bool set(int layer, int x, int y, TileCell cell);   // false si hors carte ou inchangé
    void fill(int layer, TileCell cell);

    const Tileset* tileset(int slot) const { return (slot >= 0 && slot < int(tilesets.size())) ? tilesets[std::size_t(slot)].get() : nullptr; }
    Tileset* tileset(int slot) { return (slot >= 0 && slot < int(tilesets.size())) ? tilesets[std::size_t(slot)].get() : nullptr; }
    const Tileset* tilesetOf(TileCell c) const { return tileset(cellSlot(c)); }
    int tilesetSlot(const std::string& file) const; 
    int addTileset(std::shared_ptr<Tileset> tileset); 
    void removeTileset(int slot);
    void moveTileset(int from, int to); 

    sf::Vector2f cellCenter(int x, int y) const;
    sf::Vector2i cellAt(sf::Vector2f local) const;  
    std::array<sf::Vector2f, 4> cellCorners(int x, int y) const; 
    sf::Vector2f localSize() const;                 
    sf::Vector2f localOrigin() const;

    // contour au format de cellCorners, tous layers confondus
    std::vector<std::array<sf::Vector2f, 4>> solidAreas() const;
    int usedCells(int slot) const;

    std::string resolve(const std::string& path) const;   

    uint32_t version = 0;
    bool modified = false;    

    bool fromJson(const std::string& text, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);

private:
    int m_width = 0, m_height = 0;
    std::vector<TileLayer> m_layers;
};

bool loadTileMap(TileMapData& map, const std::string& path, const std::string& baseDir, std::string* error = nullptr,
                 std::vector<std::string>* warnings = nullptr);

struct Tilemap {
    std::string file;                    
    bool visible = true;                 
    std::shared_ptr<TileMapData> data;     
};
nlohmann::ordered_json tilesToJson(const TileMapData& tileMapData, int layer);
nlohmann::ordered_json tilesetsToJson(const TileMapData& tileMapData);
bool save_maps(Registry& scene, std::string path, std::string* error = nullptr);   
bool save_map(TileMapData& tileMap, const std::string& path);
void load_map();

const char* toString(TileMapOrientation o);
const char* toString(TileLayerMode m);
