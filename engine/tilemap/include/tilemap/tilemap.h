#pragma once
// Tilemap « data-driven » : rien n'est codé en dur.
//
//  - La carte (orientation, taille, taille de case) est un fichier .map (JSON).
//  - Ses images viennent de TILESETS (tilemap/tileset.h) : une feuille PNG découpée en grille + un .tileset.
//    Un tileset est simple (on choisit la tile dans la grille) ou AUTOTILE (les transitions sont choisies seules).
//  - Chaque case contient (tileset, tile) : une tile précise de la feuille, ou « autotile » (le terrain du
//    tileset, dont la pièce est choisie selon les voisines).
//  - L'ordre des tilesets dans la carte = l'empilement : un autotile plus bas dans la liste déborde
//    sur les cases des tilesets d'avant.
//  - Une tilemap est un COMPOSANT (Tilemap) posé sur une entité : pas d'entité = pas de tilemap.
//    Transform2D de l'entité = position de la carte dans le monde.
//
// Les chemins sont relatifs au dossier du projet (là où est le .angproj).
#include "tilemap/tileset.h"

#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class TileMapOrientation {
    Orthogonal,   // cases carrées / rectangulaires
    Isometric,    // losanges : case (x, y) -> écran ((x - y) * w/2, (x + y) * h/2)
};

// ---- Contenu d'une case ---------------------------------------------------------------------------
// 0 = vide ; sinon (n° du tileset + 1) << 16 | n° de la tile dans la feuille (kAutoTile = autotile).
using TileCell = uint32_t;
inline constexpr uint16_t kAutoTile = 0xFFFF;
inline constexpr TileCell makeCell(int slot, int tile) {
    return slot < 0 ? 0u : (TileCell(slot + 1) << 16) | TileCell(uint16_t(tile));
}
inline constexpr int cellSlot(TileCell c) { return int(c >> 16) - 1; }          // -1 = vide
inline constexpr int cellTile(TileCell c) { return int(c & 0xFFFF); }
inline constexpr bool cellIsAuto(TileCell c) { return c != 0 && (c & 0xFFFF) == kAutoTile; }

class TileMapData {
public:
    TileMapOrientation orientation = TileMapOrientation::Orthogonal;
    sf::Vector2f tileSize{ 32.f, 32.f };    // taille d'une case dans le monde (iso : largeur et hauteur du losange)

    // Tilesets utilisés par la carte (chargés depuis leurs .tileset). L'ordre = l'empilement.
    std::vector<std::shared_ptr<Tileset>> tilesets;

    // Dossier du projet : sert à retrouver les fichiers (chemins relatifs). Rempli au chargement.
    std::string baseDir;

    // ---- Cases ----------------------------------------------------------------------------------
    int width() const { return m_width; }
    int height() const { return m_height; }
    void resize(int width, int height);     // garde le contenu (coin haut-gauche)
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }
    TileCell get(int x, int y) const { return inBounds(x, y) ? m_cells[std::size_t(y) * m_width + x] : 0; }
    bool set(int x, int y, TileCell cell);  // false si hors carte ou inchangé
    void fill(TileCell cell);

    // ---- Tilesets ---------------------------------------------------------------------------------
    const Tileset* tileset(int slot) const {
        return (slot >= 0 && slot < int(tilesets.size())) ? tilesets[std::size_t(slot)].get() : nullptr;
    }
    Tileset* tileset(int slot) {
        return (slot >= 0 && slot < int(tilesets.size())) ? tilesets[std::size_t(slot)].get() : nullptr;
    }
    const Tileset* tilesetOf(TileCell c) const { return tileset(cellSlot(c)); }
    int tilesetSlot(const std::string& file) const;    // -1 si absent
    int addTileset(std::shared_ptr<Tileset> tileset);  // renvoie son n° (déjà présent : le n° existant)
    void removeTileset(int slot);                      // ses cases deviennent vides
    void moveTileset(int from, int to);                // ordre d'empilement (les cases suivent)

    // ---- Repères (coordonnées LOCALES à la carte, sans le Transform2D) ----------------------------
    sf::Vector2f cellCenter(int x, int y) const;
    sf::Vector2i cellAt(sf::Vector2f local) const;     // case sous un point (peut être hors carte)
    std::array<sf::Vector2f, 4> cellCorners(int x, int y) const;   // contour (losange en iso)
    sf::Vector2f localSize() const;                    // boîte englobante de la carte
    sf::Vector2f localOrigin() const;                  // coin haut-gauche de cette boîte

    std::string resolve(const std::string& path) const;   // chemin relatif au projet -> chemin complet

    // Incrémenté à chaque modification (cases, tilesets, taille) : le rendu et l'éditeur s'en servent
    uint32_t version = 0;
    bool modified = false;    // modifications non enregistrées

    // ---- Fichier .map (JSON) ----------------------------------------------------------------------
    // fromJson charge aussi les .tileset (introuvable : gardé « missing », message dans `warnings`)
    bool fromJson(const std::string& text, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);

private:
    int m_width = 0, m_height = 0;
    std::vector<TileCell> m_cells;
};

bool loadTileMap(TileMapData& map, const std::string& path, const std::string& baseDir, std::string* error = nullptr,
                 std::vector<std::string>* warnings = nullptr);

// Composant : une tilemap dans la scène. Ajouter / retirer ce composant = activer / supprimer la tilemap.
struct Tilemap {
    std::string file;                      // .map, relatif au projet (ex. "maps/village.map")
    bool visible = true;                   // décocher pour la masquer sans la supprimer
    std::shared_ptr<TileMapData> data;     // chargée depuis `file` (loadTilemaps, scene/tilemaps.h)
};

const char* toString(TileMapOrientation o);
