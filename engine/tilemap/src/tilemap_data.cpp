#include "tilemap/tilemap.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
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
std::vector<std::string> strings(const Json& j) {
    std::vector<std::string> out;
    if (j.is_string()) out.push_back(j.get<std::string>());
    else if (j.is_array()) for (const Json& s : j) if (s.is_string()) out.push_back(s.get<std::string>());
    return out;
}
} // namespace

const char* toString(TileMapOrientation o) { return o == TileMapOrientation::Isometric ? "isometric" : "orthogonal"; }
const char* toString(AutotileMode m) {
    switch (m) {
    case AutotileMode::Corners16: return "corners16";
    case AutotileMode::Edges16:   return "edges16";
    default:                      return "none";
    }
}

// ---- Cases -------------------------------------------------------------------------------------

void TileMapData::resize(int width, int height) {
    width = std::max(0, width);
    height = std::max(0, height);
    if (width == m_width && height == m_height) return;
    std::vector<uint16_t> cells(std::size_t(width) * height, 0);
    for (int y = 0; y < std::min(height, m_height); ++y)
        for (int x = 0; x < std::min(width, m_width); ++x)
            cells[std::size_t(y) * width + x] = m_cells[std::size_t(y) * m_width + x];
    m_cells = std::move(cells);
    m_width = width;
    m_height = height;
    ++version;
    modified = true;
}

bool TileMapData::set(int x, int y, uint16_t id) {
    if (!inBounds(x, y)) return false;
    uint16_t& c = m_cells[std::size_t(y) * m_width + x];
    if (c == id) return false;
    c = id;
    ++version;
    modified = true;
    return true;
}

void TileMapData::fill(uint16_t id) {
    std::fill(m_cells.begin(), m_cells.end(), id);
    ++version;
    modified = true;
}

uint16_t TileMapData::tileId(const std::string& name) const {
    for (std::size_t i = 0; i < tiles.size(); ++i)
        if (tiles[i].name == name) return uint16_t(i + 1);
    return 0;
}

void TileMapData::removeTile(uint16_t id) {
    if (id < 1 || id > tiles.size()) return;
    const std::string name = tiles[id - 1].name;
    tiles.erase(tiles.begin() + (id - 1));
    for (uint16_t& c : m_cells) {
        if (c == id) c = 0;
        else if (c > id) --c;   // les ids suivants reculent d'un cran
    }
    for (TileDef& t : tiles) if (t.base == name) t.base.clear();
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

// ---- JSON ----------------------------------------------------------------------------------------

std::string TileMapData::toJson() const {
    Json j;
    j["format"] = "angeVault.tilemap";
    j["version"] = 1;
    j["orientation"] = toString(orientation);
    j["size"] = { m_width, m_height };
    j["tileSize"] = vec2(tileSize);

    Json list = Json::array();
    for (const TileDef& t : tiles) {
        Json d;
        d["name"] = t.name;
        d["autotile"] = toString(t.autotile);
        d["images"] = t.images;
        if (t.autotile == AutotileMode::Edges16) {
            d["edge"] = t.edge;
            d["corner"] = t.corner;
            d["ring"] = t.ring;
        }
        if (t.autotile == AutotileMode::Corners16) {
            d["cellPixels"] = t.cellPixels;
            d["frameTime"] = t.frameTime;
        }
        if (!t.base.empty()) d["base"] = t.base;
        if (t.anchor.x >= 0.f) d["anchor"] = vec2(t.anchor);
        if (t.scale != 1.f) d["scale"] = t.scale;
        if (t.solid) d["solid"] = true;
        list.push_back(d);
    }
    j["tiles"] = list;

    // Une ligne de texte par rangée : ids séparés par des espaces, « 12*3 » = 12 fois la tile 3
    Json rows = Json::array();
    for (int y = 0; y < m_height; ++y) {
        std::ostringstream row;
        int x = 0;
        while (x < m_width) {
            const uint16_t id = m_cells[std::size_t(y) * m_width + x];
            int run = 1;
            while (x + run < m_width && m_cells[std::size_t(y) * m_width + x + run] == id) ++run;
            if (x > 0) row << ' ';
            if (run > 2) row << run << '*' << id;
            else for (int k = 0; k < run; ++k) row << (k ? " " : "") << id;
            x += run;
        }
        rows.push_back(row.str());
    }
    j["cells"] = rows;
    return j.dump(2) + "\n";
}

bool TileMapData::fromJson(const std::string& text, std::string* error) {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { setError(error, "JSON invalide"); return false; }
    if (j.value("format", std::string()) != "angeVault.tilemap") { setError(error, "ce n'est pas une tilemap AngeVault"); return false; }
    if (j.value("version", 1) > 1) { setError(error, "tilemap créée par une version plus récente du moteur"); return false; }

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
    if (j.contains("tiles") && j["tiles"].is_array())
        for (const Json& d : j["tiles"]) {
            TileDef t;
            t.name = d.value("name", t.name);
            const std::string mode = d.value("autotile", std::string("none"));
            t.autotile = mode == "corners16" ? AutotileMode::Corners16 : mode == "edges16" ? AutotileMode::Edges16 : AutotileMode::None;
            t.images = strings(d.value("images", Json()));
            const auto e = strings(d.value("edge", Json()));
            const auto c = strings(d.value("corner", Json()));
            for (std::size_t i = 0; i < 4; ++i) {
                if (i < e.size()) t.edge[i] = e[i];
                if (i < c.size()) t.corner[i] = c[i];
            }
            t.ring = strings(d.value("ring", Json()));
            t.cellPixels = std::max(1, d.value("cellPixels", t.cellPixels));
            t.frameTime = d.value("frameTime", t.frameTime);
            t.base = d.value("base", std::string());
            t.anchor = vec2(d.value("anchor", Json()), t.anchor);
            t.scale = d.value("scale", t.scale);
            t.solid = d.value("solid", false);
            m.tiles.push_back(std::move(t));
        }

    m.m_width = std::max(0, w);
    m.m_height = std::max(0, h);
    m.m_cells.assign(std::size_t(m.m_width) * m.m_height, 0);
    if (j.contains("cells") && j["cells"].is_array()) {
        const Json& rows = j["cells"];
        for (int y = 0; y < m.m_height && y < int(rows.size()); ++y) {
            if (!rows[y].is_string()) continue;
            std::istringstream in(rows[y].get<std::string>());
            std::string token;
            int x = 0;
            while (in >> token && x < m.m_width) {
                int count = 1;
                long id = 0;
                const auto star = token.find('*');
                try {
                    if (star != std::string::npos) { count = std::stoi(token.substr(0, star)); id = std::stol(token.substr(star + 1)); }
                    else id = std::stol(token);
                } catch (...) { continue; }
                if (id < 0 || id > long(m.tiles.size())) id = 0;   // tile inconnue : case vide
                for (int k = 0; k < count && x < m.m_width; ++k) m.m_cells[std::size_t(y) * m.m_width + x++] = uint16_t(id);
            }
        }
    }
    m.version = version + 1;
    *this = std::move(m);
    return true;
}

bool loadTileMap(TileMapData& map, const std::string& path, const std::string& baseDir, std::string* error) {
    std::ifstream in(toPath(path), std::ios::binary);
    if (!in) { setError(error, "impossible d'ouvrir " + path); return false; }
    std::ostringstream ss;
    ss << in.rdbuf();
    map.baseDir = baseDir;
    if (!map.fromJson(ss.str(), error)) { if (error) *error = path + " : " + *error; return false; }
    map.modified = false;
    return true;
}

bool saveTileMap(TileMapData& map, const std::string& path, std::string* error) {
    const fs::path target = toPath(path);
    std::error_code ec;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { setError(error, "impossible d'écrire " + path); return false; }
        out << map.toJson();
        if (!out) { setError(error, "écriture interrompue : " + path); return false; }
    }
    fs::rename(tmp, target, ec);
    if (ec) { fs::remove(target, ec); fs::rename(tmp, target, ec); }
    if (ec) { setError(error, "impossible de remplacer " + path); return false; }
    map.modified = false;
    return true;
}
