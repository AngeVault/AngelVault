#pragma once
// Tileset : UNE feuille d'images (PNG) découpée en grille, décrite par un fichier .tileset (JSON).
//
//  - Simple   (AutotileMode::None) : on choisit la tile dans la grille de la feuille (ou un bloc de
//               plusieurs tiles) et on la pose telle quelle.
//  - Autotile (Edges16 / Corners16) : on peint le « terrain » et les transitions sont choisies seules.
//               Le rôle de chaque tile de la feuille (case pleine, 1 bord, 2 bords...) est noté dans le
//               fichier : n'importe quelle feuille peut servir, il suffit de lui dire où sont les pièces.
//
// Toutes les tiles restent pickables à la main, même dans un tileset autotile.
// Les chemins (image, .tileset) sont relatifs au dossier du projet (là où est le .angproj).
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class AutotileMode {
    None,         // tileset simple : on pose la tile choisie
    Corners16,    // feuille « dual-grid » : 16 colonnes (une par combinaison des 4 coins) x lignes (variantes
                  // ou images d'animation). Orthogonal (en isométrique : la pièce pleine, colonne 15).
    Edges16,      // pièces posées sur les cases VOISINES selon les 4 bords : 1 bord, 2 bords adjacents, 4 bords.
                  // Format des packs iso (herbe qui déborde sur la terre). Orthogonal ou isométrique.
};

// Directions des bords d'une case (voisins). En isométrique : 0 = NE, 1 = SE, 2 = SO, 3 = NO.
//                                         En orthogonal  : 0 = haut, 1 = droite, 2 = bas, 3 = gauche.
inline constexpr std::array<sf::Vector2i, 4> kTileEdgeDirs = { sf::Vector2i{ 0, -1 }, sf::Vector2i{ 1, 0 },
                                                               sf::Vector2i{ 0, 1 }, sf::Vector2i{ -1, 0 } };

struct Tileset {
    std::string name = "Tileset";
    std::string image;                      // feuille PNG
    sf::Vector2i tileSize{ 32, 32 };        // taille d'une tile DANS LA FEUILLE (px)
    int margin = 0;                         // bord autour de la grille (px)
    int spacing = 0;                        // espace entre deux tiles (px)
    int columns = 1;                        // tiles par ligne de la feuille
    int count = 0;                          // nombre de tiles (columns x lignes)

    // Placement dans le monde : point de la tile (px) posé au centre de la case, et échelle.
    // anchor (-1, -1) = automatique : centre de la tile (orthogonal : la tile est étirée à la case).
    sf::Vector2f anchor{ -1.f, -1.f };
    float scale = 1.f;
    bool solid = false;                     // bloque le passage (physique / IA, plus tard)

    // Tri en Y (arbres, murs, rochers...) : les tiles ne sont pas dessinées avec le sol mais triées avec
    // les personnages selon leur « point de tri ». Un personnage dont les pieds sont plus haut que ce
    // point passe DERRIÈRE la tile, plus bas il passe DEVANT.
    //   point de tri = centre de la case (isométrique) ou bas de la case (orthogonal) + sortOffset
    bool ySort = false;
    float sortOffset = 0.f;                 // décalage vertical du point de tri, en unités monde

    // ---- Autotile -------------------------------------------------------------------------------
    AutotileMode autotile = AutotileMode::None;
    // Edges16 : index des tiles de la feuille (-1 = aucune)
    std::vector<int> full;                  // case pleine (plusieurs = variantes au hasard, stables)
    std::array<int, 4> edge{ -1, -1, -1, -1 };     // couvre 1 bord (index = direction, voir kTileEdgeDirs)
    std::array<int, 4> corner{ -1, -1, -1, -1 };   // couvre 2 bords : le bord i et le bord i+1
    std::vector<int> ring;                  // couvre les 4 bords (facultatif : sinon 2 « coins »)
    std::vector<int> under;                 // dessiné SOUS la case pleine (ex. bloc de terre sous l'herbe)
    // Corners16 : feuille de 16 colonnes ; frameTime > 0 = les lignes sont les images d'une animation
    float frameTime = 0.f;

    // .tileset d'où vient ce tileset (relatif au projet). Rempli au chargement / à l'enregistrement.
    std::string file;
    bool missing = false;                   // fichier introuvable : gardé pour ne pas perdre la référence

    // ---- Grille ---------------------------------------------------------------------------------
    int rows() const { return columns > 0 ? (count + columns - 1) / columns : 0; }
    bool validTile(int index) const { return index >= 0 && index < count; }
    sf::IntRect rect(int index) const;                    // morceau de la feuille (px)
    sf::Vector2i tileCoords(int index) const { return { index % std::max(1, columns), index / std::max(1, columns) }; }
    int tileIndex(int col, int row) const;               // -1 si hors feuille
    // Calcule columns / count d'après la taille de l'image (et tileSize / margin / spacing)
    void computeGrid(sf::Vector2u imageSize);

    bool isAutotile() const { return autotile != AutotileMode::None; }
    // Tile qui représente le tileset (aperçu, pinceau autotile) : la case pleine
    int previewTile() const;
    // Remplit full / edge / corner / ring / under avec la disposition standard (Edges16) :
    //   4 colonnes ; ligne 0 = case pleine (variantes) ; ligne 1 = 1 bord (NE, SE, SO, NO) ;
    //   ligne 2 = 2 bords ; ligne 3 = 4 bords (variantes) ; ligne 4 = dessous (variantes).
    void applyStandardLayout();

    std::string toJson() const;
    bool fromJson(const std::string& text, std::string* error = nullptr);
};

// `path` = chemin complet du fichier. Le champ `file` (relatif au projet) n'est pas touché.
bool loadTileset(Tileset& tileset, const std::string& path, std::string* error = nullptr);
bool saveTileset(Tileset& tileset, const std::string& path, std::string* error = nullptr);

const char* toString(AutotileMode m);
AutotileMode autotileFromString(const std::string& s);
