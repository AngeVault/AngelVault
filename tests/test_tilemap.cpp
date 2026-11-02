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

    // ---- Cases, redimensionnement, suppression de tile
    TileMapData m;
    m.tiles.resize(3);
    m.tiles[0].name = "Terre";
    m.tiles[1].name = "Sable";
    m.tiles[2].name = "Herbe";
    m.tiles[2].base = "Sable";
    m.resize(5, 4);
    m.fill(1);
    CHECK(m.set(2, 2, 3) && !m.set(2, 2, 3) && !m.set(9, 9, 1));
    m.set(1, 1, 2);
    m.resize(3, 3);                          // garde le coin haut-gauche
    CHECK(m.width() == 3 && m.get(2, 2) == 3 && m.get(1, 1) == 2);
    m.removeTile(2);                         // « Sable » : ses cases deviennent vides, Herbe passe de 3 à 2
    CHECK(m.tiles.size() == 2 && m.get(1, 1) == 0 && m.get(2, 2) == 2 && m.tile(2)->name == "Herbe");
    CHECK(m.tile(2)->base.empty());          // son « dessous » a disparu avec elle
    CHECK(m.tileId("Herbe") == 2 && m.tileId("?") == 0);

    // ---- Fichier .map : aller-retour complet
    TileMapData a;
    a.orientation = TileMapOrientation::Isometric;
    a.tileSize = { 128.f, 64.f };
    TileDef grass;
    grass.name = "Herbe";
    grass.autotile = AutotileMode::Edges16;
    grass.images = { "a/full_N.png", "a/full_E.png" };
    grass.edge = { "e0", "e1", "e2", "e3" };
    grass.corner = { "c0", "c1", "c2", "c3" };
    grass.ring = { "r" };
    grass.base = "Terre";
    grass.anchor = { 128.f, 208.f };
    TileDef water;
    water.name = "Eau";
    water.autotile = AutotileMode::Corners16;
    water.images = { "water.png" };
    water.frameTime = 0.15f;
    water.solid = true;
    a.tiles = { water, grass };
    a.resize(40, 3);
    a.fill(1);
    for (int x = 10; x < 13; ++x) a.set(x, 1, 2);
    a.set(39, 2, 0);
    const std::string json = a.toJson();
    TileMapData b;
    std::string err;
    CHECK(b.fromJson(json, &err));
    CHECK(b.toJson() == json);
    CHECK(b.orientation == TileMapOrientation::Isometric && b.width() == 40 && b.height() == 3);
    CHECK(b.get(11, 1) == 2 && b.get(9, 1) == 1 && b.get(39, 2) == 0);
    CHECK(b.tiles[1].autotile == AutotileMode::Edges16 && b.tiles[1].corner[3] == "c3" && b.tiles[1].ring.size() == 1);
    CHECK(b.tiles[1].anchor == sf::Vector2f(128.f, 208.f) && b.tiles[1].base == "Terre");
    CHECK(b.tiles[0].autotile == AutotileMode::Corners16 && b.tiles[0].solid && std::abs(b.tiles[0].frameTime - 0.15f) < 1e-6f);
    CHECK(json.find("\"40*1\"") != std::string::npos);   // RLE : une rangée de 40 fois la tile 1
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
