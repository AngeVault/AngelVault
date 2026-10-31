#pragma once
// Tilemap « data-driven » : rien n'est codé en dur.
//
//  - La carte (orientation, taille, taille de case) et ses TILES sont dans un fichier .map (JSON).
//  - Chaque tile est définie par l'utilisateur (éditeur : panneau Tile Palette > + Tile) :
//    simple (une image, ou plusieurs variantes) ou AUTOTILE (les transitions sont choisies seules).
//  - Une tilemap est un COMPOSANT (Tilemap) posé sur une entité : pas d'entité = pas de tilemap.
//    Transform2D de l'entité = position de la carte dans le monde.
//
// Les chemins d'images sont relatifs au dossier du projet (là où est le .angproj).
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

enum class AutotileMode {
    None,         // tile simple : une image (plusieurs = variantes choisies au hasard, mais toujours les mêmes)
    Corners16,    // feuille « dual-grid » : 16 colonnes (une par combinaison des 4 coins) x lignes (variantes
                  // ou images d'animation). Orthogonal.
    Edges16,      // pièces posées sur les cases VOISINES selon les 4 bords : 1 bord, 2 bords adjacents, 4 bords.
                  // C'est le format des packs iso (herbe qui déborde sur la terre). Orthogonal ou isométrique.
};

// Directions des bords d'une case (voisins). En isométrique : 0 = NE, 1 = SE, 2 = SO, 3 = NO.
//                                         En orthogonal  : 0 = haut, 1 = droite, 2 = bas, 3 = gauche.
inline constexpr std::array<sf::Vector2i, 4> kTileEdgeDirs = { sf::Vector2i{ 0, -1 }, sf::Vector2i{ 1, 0 },
                                                               sf::Vector2i{ 0, 1 }, sf::Vector2i{ -1, 0 } };

struct TileDef {
    std::string name = "Tile";
    AutotileMode autotile = AutotileMode::None;
    std::vector<std::string> images;        // None / Edges16 : image(s) de la case pleine ; Corners16 : feuille(s)

    // Edges16 : pièces indexées par direction de bord (0..3, voir kTileEdgeDirs)
    std::array<std::string, 4> edge;        // couvre 1 bord
    std::array<std::string, 4> corner;      // couvre 2 bords : le bord i et le bord i+1
    std::vector<std::string> ring;          // couvre les 4 bords (optionnel : sinon 2 « corner »)

    // Corners16
    int cellPixels = 16;                    // taille d'une cellule de la feuille (px)
    float frameTime = 0.f;                  // > 0 : les lignes de la feuille sont des images d'animation

    std::string base;                       // tile dessinée DESSOUS (ex. « Terre » sous « Herbe »), "" = aucune
    // Placement de l'image : point de l'image (px) posé au centre de la case, et échelle.
    // anchor (-1, -1) = automatique : centre de l'image (orthogonal : l'image est étirée à la case).
    sf::Vector2f anchor{ -1.f, -1.f };
    float scale = 1.f;
    bool solid = false;                     // bloque le passage (pour la physique / l'IA, plus tard)
};

class TileMapData {
public:
    TileMapOrientation orientation = TileMapOrientation::Orthogonal;
    sf::Vector2f tileSize{ 32.f, 32.f };    // taille d'une case dans le monde (iso : largeur et hauteur du losange)
    std::vector<TileDef> tiles;             // id d'une tile = son index + 1 (0 = case vide). L'ordre = l'empilement.

    // Dossier du projet : sert à retrouver les images (chemins relatifs). Rempli au chargement.
    std::string baseDir;

    // ---- Cases ----------------------------------------------------------------------------------
    int width() const { return m_width; }
    int height() const { return m_height; }
    void resize(int width, int height);     // garde le contenu (coin haut-gauche)
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }
    uint16_t get(int x, int y) const { return inBounds(x, y) ? m_cells[std::size_t(y) * m_width + x] : 0; }
    bool set(int x, int y, uint16_t tileId);   // false si hors carte ou inchangé
    void fill(uint16_t tileId);

    // ---- Tiles ------------------------------------------------------------------------------------
    const TileDef* tile(uint16_t id) const { return (id >= 1 && id <= tiles.size()) ? &tiles[id - 1] : nullptr; }
    uint16_t tileId(const std::string& name) const;   // 0 si inconnue
    void removeTile(uint16_t id);                      // les cases de cette tile deviennent vides

    // ---- Repères (coordonnées LOCALES à la carte, sans le Transform2D) ----------------------------
    sf::Vector2f cellCenter(int x, int y) const;
    sf::Vector2i cellAt(sf::Vector2f local) const;     // case sous un point (peut être hors carte)
    std::array<sf::Vector2f, 4> cellCorners(int x, int y) const;   // contour (losange en iso)
    sf::Vector2f localSize() const;                    // boîte englobante de la carte
    sf::Vector2f localOrigin() const;                  // coin haut-gauche de cette boîte

    std::string resolve(const std::string& path) const;   // chemin d'image -> chemin complet

    // Incrémenté à chaque modification (cases, tiles, taille) : le rendu et l'éditeur s'en servent
    uint32_t version = 0;
    bool modified = false;    // modifications non enregistrées

    // ---- Fichier .map (JSON) ----------------------------------------------------------------------
    std::string toJson() const;
    bool fromJson(const std::string& text, std::string* error = nullptr);

private:
    int m_width = 0, m_height = 0;
    std::vector<uint16_t> m_cells;
};

bool loadTileMap(TileMapData& map, const std::string& path, const std::string& baseDir, std::string* error = nullptr);
bool saveTileMap(TileMapData& map, const std::string& path, std::string* error = nullptr);

// Composant : une tilemap dans la scène. Ajouter / retirer ce composant = activer / supprimer la tilemap.
struct Tilemap {
    std::string file;                      // .map, relatif au projet (ex. "maps/village.map")
    bool visible = true;                   // décocher pour la masquer sans la supprimer
    std::shared_ptr<TileMapData> data;     // chargée depuis `file` (loadTilemaps, scene/tilemaps.h)
};

const char* toString(TileMapOrientation o);
const char* toString(AutotileMode m);
