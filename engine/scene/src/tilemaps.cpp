#include "scene/tilemaps.h"
#include "scene/project.h"

#include <filesystem>

namespace {
std::string join(const std::string& dir, const std::string& rel) {
    const std::filesystem::path p(std::u8string(rel.begin(), rel.end()));
    if (p.is_absolute() || dir.empty()) return rel;
    const std::filesystem::path full = std::filesystem::path(std::u8string(dir.begin(), dir.end())) / p;
    const std::u8string s = full.lexically_normal().generic_u8string();
    return std::string(s.begin(), s.end());
}
} // namespace

void loadTilemaps(Registry& registry, const std::string& projectDir, std::vector<std::string>* warnings) {
    registry.view<Tilemap>([&](Entity, Tilemap& tm) {
        if (tm.data || tm.file.empty()) return;
        auto data = std::make_shared<TileMapData>();
        std::string error;
        if (loadTileMap(*data, join(projectDir, tm.file), projectDir, &error)) tm.data = std::move(data);
        else if (warnings) warnings->push_back("Tilemap : " + error);
    });
}

bool saveTilemaps(Registry& registry, const std::string& projectDir, bool all, std::string* error) {
    bool ok = true;
    registry.view<Tilemap>([&](Entity, Tilemap& tm) {
        if (!tm.data || tm.file.empty() || (!all && !tm.data->modified)) return;
        std::string err;
        if (!saveTileMap(*tm.data, join(projectDir, tm.file), &err)) { ok = false; if (error) *error = err; }
    });
    return ok;
}

std::shared_ptr<TileMapData> makeTileMap(TileMapOrientation orientation, int width, int height, const std::string& projectDir) {
    auto map = std::make_shared<TileMapData>();
    map->orientation = orientation;
    map->tileSize = orientation == TileMapOrientation::Isometric ? sf::Vector2f{ 128.f, 64.f } : sf::Vector2f{ 32.f, 32.f };
    map->baseDir = projectDir;
    map->resize(width, height);
    map->modified = true;
    return map;
}
