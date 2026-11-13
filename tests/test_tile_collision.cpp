#include "tilemap/tilemap.h"

#include <cstdio>
#include <memory>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

static TileMapData makeMap(TileMapOrientation o) {
    TileMapData m;
    m.orientation = o;
    m.tileSize = o == TileMapOrientation::Isometric ? sf::Vector2f{ 128.f, 64.f } : sf::Vector2f{ 32.f, 32.f };
    m.resize(6, 5);
    auto ground = std::make_shared<Tileset>();
    auto wall = std::make_shared<Tileset>();
    ground->file = "ground.tileset";
    wall->file = "wall.tileset";
    wall->solid = true;
    m.addTileset(ground);
    m.addTileset(wall);
    m.fill(makeCell(0, 0));
    return m;
}

int main() {
    {  
        TileMapData m = makeMap(TileMapOrientation::Orthogonal);
        CHECK(m.solidAreas().empty());
    }
    {
        TileMapData m = makeMap(TileMapOrientation::Orthogonal);
        for (int y = 1; y <= 2; ++y)
            for (int x = 1; x <= 3; ++x) m.set(x, y, makeCell(1, 0));
        m.set(5, 4, makeCell(1, 0));
        const auto areas = m.solidAreas();
        CHECK(areas.size() == 2);
        CHECK(areas[0][0] == sf::Vector2f(32.f, 32.f) && areas[0][2] == sf::Vector2f(128.f, 96.f));
        CHECK(areas[1][0] == sf::Vector2f(160.f, 128.f) && areas[1][2] == sf::Vector2f(192.f, 160.f));
    }
    { 
        TileMapData m = makeMap(TileMapOrientation::Orthogonal);
        m.set(0, 0, makeCell(1, 0));
        m.set(1, 0, makeCell(1, 0));
        m.set(0, 1, makeCell(1, 0));
        CHECK(m.solidAreas().size() == 2);
    }
    {  
        TileMapData m = makeMap(TileMapOrientation::Isometric);
        for (int x = 0; x < 3; ++x) m.set(x, 0, makeCell(1, 0));
        const auto areas = m.solidAreas();
        CHECK(areas.size() == 1);
        CHECK(areas[0][0] == m.cellCorners(0, 0)[0]);
        CHECK(areas[0][1] == m.cellCorners(2, 0)[1]);
        CHECK(areas[0][2] == m.cellCorners(2, 0)[2]);
        CHECK(areas[0][3] == m.cellCorners(0, 0)[3]);
    }

    if (g_failures == 0) std::printf("test_tile_collision : OK\n");
    return g_failures == 0 ? 0 : 1;
}
