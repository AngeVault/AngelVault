#include "save_map.h"

#include <iostream>

#include "fstream"

using json = nlohmann::json;

void save_map(EditorContext& ctx) {
    json map_save;
    map_save = {
        "format:", "angeVault.map",
        {"version:" ,1},
        {"width:" ,ctx.map().width},
        {"height:" ,ctx.map().height},
        {"tileSize:" ,ctx.map().tileSize},
        {"tileset:" ,"assets/tilesets/overworld"},
        {"types:" ,"[None", "Sand", "Water", "Grass", "BrickPath", "DirtPath", "DonjonFloor", "Wall", "Path]"},
        {"encoding:" ,"rle-base64"},
        {"tiles:" ,"AwDIAQQB...."}
    };
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
