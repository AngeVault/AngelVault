#include "save_map.h"

#include <iostream>

#include "fstream"

using json = nlohmann::json;

void save_map(EditorContext& ctx) {
    json map_save;
    // map_save = {
    //     "format:", "angeVault.map",
    //     {"version:" ,1},
    //     {"width:" ,ctx.map().width},
    //     {"height:" ,ctx.map().height},
    //     {"tileSize:" ,ctx.map().tileSize},
    //     {"tileset:" ,"assets/tilesets/overworld"},
    //     {"types:" ,"[None", "Sand", "Water", "Grass", "BrickPath", "DirtPath", "DonjonFloor", "Wall", "Path]"},
    //     {"encoding:" ,"rle-base64"},
    //     {"tiles:" ,"AwDIAQQB...."}
    // };
    map_save["format"] = "angeVault.map";
    map_save["version"] = 1;
    map_save["width"] = ctx.map().width;
    map_save["height"] = ctx.map().height;
    map_save["tileset"] = "assets/tilesets/overworld";
    map_save["tiles"] = tilesToJson(ctx.map());
    std::ofstream map_file("save_map.json");
    map_file << map_save.dump();
    map_file.close();
}

void load_map(EditorContext& ctx) {
    std::ifstream map_file("save_map.json");
    json map_save;
    map_file >> map_save;

    std::cout << map_save.dump() << std::endl;
}

nlohmann::json tilesToJson(TileMap& map) {
    json tiles = json::array();
    for (int i = 0; i < map.width; ++i) {
        for (int j = 0; j < map.height; ++j) {
            tiles.push_back(map.getType(i,j));
        }
    }
    return tiles;
}
