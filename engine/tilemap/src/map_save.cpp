#include "map_save.h"

using json = nlohmann::json;

void save_editor_context(EditorContext& ctx) {
    json map_save;
    map_save = {
        "format:" "angeVault.map",
        "version:" "1",
        "width:" ,ctx.map().width,
        "height:" ,ctx.map().height,
        "tileSize:" "16",
        "tileset:" "assets/tilesets/overworld",
        "types:" "[None", "Sand", "Water", "Grass", "BrickPath", "DirtPath", "DonjonFloor", "Wall", "Path]",
        "encoding:" "rle-base64",
        "tiles:" "AwDIAQQB...."
    };
}

EditorContext load_editor_context() {
}
