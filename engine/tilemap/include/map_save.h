#pragma once
#include <nlohmann/json.hpp>
#include "RtsTile_map.hpp"
#include <../../../editor/src/editor_context.h>

void save_editor_context(EditorContext& ctx);
EditorContext load_editor_context();
