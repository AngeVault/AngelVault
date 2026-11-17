// Tests de la tilemap « data-driven » (tilemap/tilemap.h)
#include "tilemap/tilemap.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

int main() {
    // ---- Conversions case <-> monde, orthogonal et isométrique (aller-retour sur toutes les cases)
    for (auto o : { TileMapOrientation::Orthogonal, TileMapOrientation::Isometric }) {
        TileMapData m;
        m.orientation = o;
        m.tileSize = o == TileMapOrientation::Isometric ? sf::Vector2f{ 128.f, 64.f } : sf::Vector2f{ 32.f, 16.f };
        m.resize(10, 7);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 10; ++x) {
                CHECK(m.cellAt(m.cellCenter(x, y)) == sf::Vector2i(x, y));
                // un point près d'un coin, mais à l'intérieur, reste dans la case
                const auto c = m.cellCorners(x, y);
                const sf::Vector2f inside = c[0] + (m.cellCenter(x, y) - c[0]) * 0.1f;
                CHECK(m.cellAt(inside) == sf::Vector2i(x, y));
            }
    }
    {   // iso : losange de la case (0,0) avec sa pointe haute en (0,0)
        TileMapData m;
        m.orientation = TileMapOrientation::Isometric;
        m.tileSize = { 128.f, 64.f };
        m.resize(4, 3);
        CHECK(m.cellCenter(0, 0) == sf::Vector2f(0.f, 32.f));
        CHECK(m.cellCenter(1, 0) == sf::Vector2f(64.f, 64.f));    // x+1 : vers le bas-droite (SE)
        CHECK(m.cellCenter(0, 1) == sf::Vector2f(-64.f, 64.f));   // y+1 : vers le bas-gauche (SO)
        CHECK(m.localOrigin().x == -3 * 64.f && m.localSize() == sf::Vector2f(7 * 64.f, 7 * 32.f));
    }

    // ---- Layers : cases séparées, redimensionnement, ordre
    TileMapData m;
    auto ground = std::make_shared<Tileset>();
    auto wall = std::make_shared<Tileset>();
    ground->file = "ground.tileset";
    wall->file = "wall.tileset";
    m.addTileset(ground);
    m.addTileset(wall);
    m.resize(5, 4);
    CHECK(m.addLayer("fond") == 0 && m.addLayer("supérieur", TileLayerMode::Above) == 1);
    CHECK(m.addLayer("principal", TileLayerMode::YSort, 1) == 1 && m.layer(2)->name == "supérieur");
    m.fill(0, makeCell(0, 0));
    CHECK(m.set(1, 2, 2, makeCell(1, 3)) && !m.set(1, 2, 2, makeCell(1, 3)) && !m.set(1, 9, 9, 1) && !m.set(7, 0, 0, 1));
    CHECK(m.get(0, 2, 2) == makeCell(0, 0) && m.get(2, 2, 2) == 0);
    m.resize(3, 3);   // garde le coin haut-gauche, sur tous les layers
    CHECK(m.width() == 3 && m.get(1, 2, 2) == makeCell(1, 3) && m.get(0, 2, 2) == makeCell(0, 0));
    CHECK(m.usedCells(0) == 9 && m.usedCells(1) == 1);
    m.moveLayer(1, 2);
    CHECK(m.layer(2)->name == "principal" && m.get(2, 2, 2) == makeCell(1, 3));
    m.removeTileset(0);   // ses cases se vident, le tileset 1 devient 0 sur tous les layers
    CHECK(m.get(0, 0, 0) == 0 && m.get(2, 2, 2) == makeCell(0, 3));
    m.removeLayer(0);
    CHECK(m.layerCount() == 2 && m.layer(0)->name == "supérieur");

    // ---- Fichier .map : aller-retour avec plusieurs layers
    TileMapData a;
    a.orientation = TileMapOrientation::Isometric;
    a.tileSize = { 128.f, 64.f };
    a.addTileset(ground);
    a.addTileset(wall);
    a.resize(40, 3);
    a.addLayer("fond");
    a.addLayer("principal", TileLayerMode::YSort);
    a.addLayer("supérieur", TileLayerMode::Above);
    a.fill(0, makeCell(0, kAutoTile));
    for (int x = 10; x < 13; ++x) a.set(1, x, 1, makeCell(1, 4));
    a.set(2, 39, 2, makeCell(1, 7));
    a.layer(2)->hidden = true;
    const std::string file = "test_tilemap_layers.map";
    CHECK(save_map(a, file));
    TileMapData b;
    std::string err;
    CHECK(loadTileMap(b, file, "", &err));
    std::remove(file.c_str());
    CHECK(b.orientation == TileMapOrientation::Isometric && b.width() == 40 && b.height() == 3 && b.tilesets.size() == 2);
    CHECK(b.layerCount() == 3 && b.layer(1)->name == "principal" && b.layer(1)->mode == TileLayerMode::YSort);
    CHECK(b.layer(2)->mode == TileLayerMode::Above && !b.layer(2)->hidden);   // caché : éditeur seulement
    CHECK(b.get(0, 5, 0) == makeCell(0, kAutoTile) && b.get(1, 11, 1) == makeCell(1, 4) && b.get(1, 9, 1) == 0);
    CHECK(b.get(2, 39, 2) == makeCell(1, 7) && b.get(2, 38, 2) == 0);
    CHECK(tilesToJson(a, 0)[0] == "40*0:a");   // RLE : une rangée de 40 fois la même case

    // ---- Ancien format v2 : une seule grille devient le layer « fond »
    TileMapData old;
    CHECK(old.fromJson(R"({"format":"angeVault.tilemap","version":2,"size":[2,1],"tilesets":[],"cells":["2*0"]})", &err));
    CHECK(old.layerCount() == 1 && old.layer(0)->name == "fond" && old.width() == 2);
    CHECK(!b.fromJson("{ pas du json", &err) && !b.fromJson("{\"format\":\"autre\"}", &err));

    // ---- Chemins relatifs au projet
    TileMapData p;
    p.baseDir = "/projet";
    CHECK(p.resolve("assets/x.png") == "/projet/assets/x.png");
    CHECK(p.resolve("/ailleurs/y.png") == "/ailleurs/y.png");

    if (g_failures) { std::printf("%d test(s) en échec\n", g_failures); return EXIT_FAILURE; }
    std::printf("test_tilemap : OK\n");
    return EXIT_SUCCESS;
}
