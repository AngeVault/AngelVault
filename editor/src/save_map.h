#pragma once
#include <nlohmann/json.hpp>
#include <../../../editor/src/editor_context.h>

void save_map(EditorContext& ctx);
void load_map(EditorContext& ctx);
nlohmann::json tilesToJson(TileMap& map);