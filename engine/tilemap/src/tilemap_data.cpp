#include "tilemap/tilemap.h"
#include "fstream"
#include "scene/project.h"
#include "components/name.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>







using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;



namespace {
fs::path toPath(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }
std::string fromPath(const fs::path& p) {
    const std::u8string s = p.generic_u8string();
    return std::string(s.begin(), s.end());
}
void setError(std::string* e, const std::string& m) { if (e) *e = m; }

Json vec2(sf::Vector2f v) { return Json::array({ v.x, v.y }); }
sf::Vector2f vec2(const Json& j, sf::Vector2f fallback) {
    if (!j.is_array() || j.size() < 2 || !j[0].is_number() || !j[1].is_number()) return fallback;
    return { j[0].get<float>(), j[1].get<float>() };
}
} // namespace

const char* toString(TileMapOrientation o) { return o == TileMapOrientation::Isometric ? "isometric" : "orthogonal"; }



bool save_maps(Registry& scene, std::string path) {
    scene.view<Tilemap>([&] (Entity& e, Tilemap& tileMap) {
        save_map(*tileMap.data,path+"/maps/"+ scene.getComponent<NameComponent>(e).name +".map");
    });
    // load_map();
    return true;
}

std::string cellToken(TileCell& cell) {
    std::string token = std::to_string(cellSlot(cell));
    if (cell == 0)return "0";
    if (cellIsAuto(cell))
        token+=":a";
    else {
        token += ":"+std::to_string(cellTile(cell));
    }
    return token;
}

nlohmann::ordered_json tilesToJson(TileMapData& tileMapData) {
Json tiles = Json::array();
    for (int i = 0; i < tileMapData.height(); ++i) {
        std::string row;
        int j = 0;
        while (j < tileMapData.width() ) {
            int count = 1;
            TileCell cell = tileMapData.get(j,i);
            while (j+count <= tileMapData.width() && cell == tileMapData.get(j+count, i)) {
                count++;
            }
            if (!row.empty())row+=" ";
            j+=count;
            row += std::to_string(count)+"*";
            row+= cellToken(cell);
        }
            tiles.push_back(row);
    }
    return tiles;
}

nlohmann::ordered_json tilesetsToJson(const TileMapData& tileMapData) {
    Json tilesets = Json::array();
    for (const auto & tileset : tileMapData.tilesets) {
            tilesets.push_back(tileset->file );
    }
    return tilesets;
}

void save_map(TileMapData& tileMap, const std::string& path) {
    Json map_save;
    map_save["format"] = "angeVault.tilemap";
    map_save["version"] = 2;
    map_save["tileSize"] = {tileMap.tileSize.x,tileMap.tileSize.y};
    map_save["size"] = {tileMap.width(),tileMap.height()};
    map_save["orientation"] = toString(TileMapOrientation::Isometric);
    map_save["tilesets"] = tilesetsToJson(tileMap);
    map_save["cells"] = tilesToJson(tileMap);
    std::ofstream map_file(path);
    map_file << map_save.dump();
    map_file.close();
}


// ---- Cases -------------------------------------------------------------------------------------

void TileMapData::resize(int width, int height) {
    width = std::max(0, width);
    height = std::max(0, height);
    if (width == m_width && height == m_height) return;
    std::vector<TileCell> cells(std::size_t(width) * height, 0);
    for (int y = 0; y < std::min(height, m_height); ++y)
        for (int x = 0; x < std::min(width, m_width); ++x)
            cells[std::size_t(y) * width + x] = m_cells[std::size_t(y) * m_width + x];
    m_cells = std::move(cells);
    m_width = width;
    m_height = height;
    ++version;
    modified = true;
}

bool TileMapData::set(int x, int y, TileCell cell) {
    if (!inBounds(x, y)) return false;
    TileCell& c = m_cells[std::size_t(y) * m_width + x];
    if (c == cell) return false;
    c = cell;
    ++version;
    modified = true;
    return true;
}

void TileMapData::fill(TileCell cell) {
    std::fill(m_cells.begin(), m_cells.end(), cell);
    ++version;
    modified = true;
}

// ---- Tilesets ----------------------------------------------------------------------------------

int TileMapData::tilesetSlot(const std::string& file) const {
    for (std::size_t i = 0; i < tilesets.size(); ++i)
        if (tilesets[i] && !file.empty() && tilesets[i]->file == file) return int(i);
    return -1;
}

int TileMapData::addTileset(std::shared_ptr<Tileset> ts) {
    if (!ts) return -1;
    if (const int existing = tilesetSlot(ts->file); existing >= 0) return existing;
    tilesets.push_back(std::move(ts));
    ++version;
    modified = true;
    return int(tilesets.size()) - 1;
}

void TileMapData::removeTileset(int slot) {
    if (slot < 0 || slot >= int(tilesets.size())) return;
    tilesets.erase(tilesets.begin() + slot);
    for (TileCell& c : m_cells) {
        const int s = cellSlot(c);
        if (s == slot) c = 0;
        else if (s > slot) c = makeCell(s - 1, cellTile(c));   // les tilesets suivants reculent d'un cran
    }
    ++version;
    modified = true;
}

void TileMapData::moveTileset(int from, int to) {
    const int n = int(tilesets.size());
    if (from < 0 || to < 0 || from >= n || to >= n || from == to) return;
    auto ts = tilesets[std::size_t(from)];
    tilesets.erase(tilesets.begin() + from);
    tilesets.insert(tilesets.begin() + to, std::move(ts));
    // nouveau n° de chaque ancien n°
    std::vector<int> remap(static_cast<std::size_t>(n));
    for (int s = 0; s < n; ++s) {
        int r = s;
        if (s == from) r = to;
        else if (from < to && s > from && s <= to) r = s - 1;
        else if (from > to && s >= to && s < from) r = s + 1;
        remap[std::size_t(s)] = r;
    }
    for (TileCell& c : m_cells)
        if (const int s = cellSlot(c); s >= 0 && s < n) c = makeCell(remap[std::size_t(s)], cellTile(c));
    ++version;
    modified = true;
}

// ---- Repères -----------------------------------------------------------------------------------

sf::Vector2f TileMapData::cellCenter(int x, int y) const {
    const float w = tileSize.x, h = tileSize.y;
    if (orientation == TileMapOrientation::Isometric)
        return { (x - y) * w * 0.5f, (x + y + 1) * h * 0.5f };
    return { (x + 0.5f) * w, (y + 0.5f) * h };
}

sf::Vector2i TileMapData::cellAt(sf::Vector2f p) const {
    const float w = std::max(tileSize.x, 0.001f), h = std::max(tileSize.y, 0.001f);
    if (orientation == TileMapOrientation::Isometric) {
        const float a = p.x / (w * 0.5f), b = p.y / (h * 0.5f);
        return { int(std::floor((a + b) * 0.5f)), int(std::floor((b - a) * 0.5f)) };
    }
    return { int(std::floor(p.x / w)), int(std::floor(p.y / h)) };
}

std::array<sf::Vector2f, 4> TileMapData::cellCorners(int x, int y) const {
    const float w = tileSize.x, h = tileSize.y;
    if (orientation == TileMapOrientation::Isometric) {
        const sf::Vector2f c = cellCenter(x, y);
        return { sf::Vector2f{ c.x, c.y - h * 0.5f }, sf::Vector2f{ c.x + w * 0.5f, c.y },
                 sf::Vector2f{ c.x, c.y + h * 0.5f }, sf::Vector2f{ c.x - w * 0.5f, c.y } };
    }
    return { sf::Vector2f{ x * w, y * h }, sf::Vector2f{ (x + 1) * w, y * h },
             sf::Vector2f{ (x + 1) * w, (y + 1) * h }, sf::Vector2f{ x * w, (y + 1) * h } };
}

sf::Vector2f TileMapData::localOrigin() const {
    if (orientation == TileMapOrientation::Isometric) return { -m_height * tileSize.x * 0.5f, 0.f };
    return { 0.f, 0.f };
}

sf::Vector2f TileMapData::localSize() const {
    if (orientation == TileMapOrientation::Isometric)
        return { (m_width + m_height) * tileSize.x * 0.5f, (m_width + m_height) * tileSize.y * 0.5f };
    return { m_width * tileSize.x, m_height * tileSize.y };
}

std::string TileMapData::resolve(const std::string& path) const {
    if (path.empty()) return path;
    const fs::path p = toPath(path);
    if (p.is_absolute() || baseDir.empty()) return path;
    return fromPath((toPath(baseDir) / p).lexically_normal());
}

// ---- JSON (lecture seule : l'enregistrement des cartes est géré ailleurs) ----------------------------------------------------------------------------------------
//
// Cases : une ligne de texte par rangée. « 0 » = vide, « 2:15 » = tileset 2 (compté depuis 0), tile 15,
// « 2:a » = tileset 2 en autotile, « 12*2:a » = 12 fois la même case.

namespace {
TileCell parseCell(const std::string& v, int slots) {
    const auto colon = v.find(':');
    if (colon == std::string::npos) return 0;   // « 0 » ou inconnu : vide
    const int slot = std::stoi(v.substr(0, colon));
    const std::string t = v.substr(colon + 1);
    if (slot < 0 || slot >= slots) return 0;     // tileset inconnu : case vide
    if (t == "a") return makeCell(slot, kAutoTile);
    const int tile = std::stoi(t);
    return (tile >= 0 && tile < kAutoTile) ? makeCell(slot, tile) : 0;
}
} // namespace

bool TileMapData::fromJson(const std::string& text, std::string* error, std::vector<std::string>* warnings) {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { setError(error, "JSON invalide"); return false; }
    if (j.value("format", std::string()) != "angeVault.tilemap") { setError(error, "ce n'est pas une tilemap AngeVault"); return false; }
    const int fileVersion = j.value("version", 1);
    if (fileVersion > 2) { setError(error, "tilemap créée par une version plus récente du moteur"); return false; }
    if (fileVersion < 2) {
        setError(error, "ancien format (une image par tile) : non pris en charge");
        return false;
    }

    TileMapData m;
    m.baseDir = baseDir;
    m.orientation = j.value("orientation", std::string()) == "isometric" ? TileMapOrientation::Isometric
                                                                         : TileMapOrientation::Orthogonal;
    m.tileSize = vec2(j.value("tileSize", Json()), m.tileSize);
    int w = 0, h = 0;
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2) {
        w = j["size"][0].get<int>();
        h = j["size"][1].get<int>();
    }
    if (j.contains("tilesets") && j["tilesets"].is_array())
        for (const Json& f : j["tilesets"]) {
            auto ts = std::make_shared<Tileset>();
            ts->file = f.is_string() ? f.get<std::string>() : std::string();
            ts->name = fromPath(toPath(ts->file).stem());
            std::string err;
            if (!loadTileset(*ts, m.resolve(ts->file), &err)) {
                ts->missing = true;   // gardé : la carte ne perd pas ses cases
                if (warnings) warnings->push_back("Tileset : " + err);
            }
            m.tilesets.push_back(std::move(ts));
        }

    m.m_width = std::max(0, w);
    m.m_height = std::max(0, h);
    m.m_cells.assign(std::size_t(m.m_width) * m.m_height, 0);
    const int slots = int(m.tilesets.size());
    if (j.contains("cells") && j["cells"].is_array()) {
        const Json& rows = j["cells"];
        for (int y = 0; y < m.m_height && y < int(rows.size()); ++y) {
            if (!rows[y].is_string()) continue;
            std::istringstream in(rows[y].get<std::string>());
            std::string token;
            int x = 0;
            while (in >> token && x < m.m_width) {
                int count = 1;
                TileCell c = 0;
                const auto star = token.find('*');
                try {
                    if (star != std::string::npos) { count = std::stoi(token.substr(0, star)); c = parseCell(token.substr(star + 1), slots); }
                    else c = parseCell(token, slots);
                } catch (...) { c = 0; }
                for (int k = 0; k < count && x < m.m_width; ++k) m.m_cells[std::size_t(y) * m.m_width + x++] = c;
            }
        }
    }
    m.version = version + 1;
    *this = std::move(m);
    return true;
}

bool loadTileMap(TileMapData& map, const std::string& path, const std::string& baseDir, std::string* error,
                 std::vector<std::string>* warnings) {
    std::ifstream in(toPath(path), std::ios::binary);
    if (!in) { setError(error, "impossible d'ouvrir " + path); return false; }
    std::ostringstream ss;
    ss << in.rdbuf();
    map.baseDir = baseDir;
    if (!map.fromJson(ss.str(), error, warnings)) { if (error) *error = path + " : " + *error; return false; }
    map.modified = false;
    return true;
}
