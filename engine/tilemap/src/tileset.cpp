#include "tilemap/tileset.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace {
fs::path toPath(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }
void setError(std::string* e, const std::string& m) { if (e) *e = m; }

std::vector<int> ints(const Json& j) {
    std::vector<int> out;
    if (j.is_number_integer()) out.push_back(j.get<int>());
    else if (j.is_array()) for (const Json& v : j) out.push_back(v.is_number_integer() ? v.get<int>() : -1);
    return out;
}
std::array<int, 4> four(const Json& j) {
    std::array<int, 4> out{ -1, -1, -1, -1 };
    const std::vector<int> v = ints(j);
    for (std::size_t i = 0; i < 4 && i < v.size(); ++i) out[i] = v[i];
    return out;
}
} // namespace

const char* toString(AutotileMode m) {
    switch (m) {
    case AutotileMode::Corners16: return "corners16";
    case AutotileMode::Edges16:   return "edges16";
    default:                      return "none";
    }
}

AutotileMode autotileFromString(const std::string& s) {
    if (s == "corners16") return AutotileMode::Corners16;
    if (s == "edges16") return AutotileMode::Edges16;
    return AutotileMode::None;
}

// ---- Grille ------------------------------------------------------------------------------------

sf::IntRect Tileset::rect(int index) const {
    const sf::Vector2i c = tileCoords(std::max(0, index));
    return sf::IntRect({ margin + c.x * (tileSize.x + spacing), margin + c.y * (tileSize.y + spacing) }, tileSize);
}

int Tileset::tileIndex(int col, int row) const {
    if (col < 0 || row < 0 || col >= columns) return -1;
    const int i = row * columns + col;
    return i < count ? i : -1;
}

void Tileset::computeGrid(sf::Vector2u size) {
    const int w = std::max(1, tileSize.x), h = std::max(1, tileSize.y);
    const int cols = (int(size.x) - 2 * margin + spacing) / (w + spacing);
    const int rows = (int(size.y) - 2 * margin + spacing) / (h + spacing);
    columns = std::max(1, cols);
    count = std::max(0, cols) * std::max(0, rows);
}

int Tileset::previewTile() const {
    switch (autotile) {
    case AutotileMode::Edges16:   return !full.empty() ? full.front() : 0;
    case AutotileMode::Corners16: return std::min(15, std::max(0, count - 1));
    default:                      return 0;
    }
}

void Tileset::applyStandardLayout() {
    auto row = [this](int r) {
        std::vector<int> out;
        for (int c = 0; c < 4; ++c) if (const int i = tileIndex(c, r); i >= 0) out.push_back(i);
        return out;
    };
    full = row(0);
    for (int i = 0; i < 4; ++i) { edge[i] = tileIndex(i, 1); corner[i] = tileIndex(i, 2); }
    ring = row(3);
    under = row(4);
}

// ---- JSON --------------------------------------------------------------------------------------

std::string Tileset::toJson() const {
    Json j;
    j["format"] = "angeVault.tileset";
    j["version"] = 1;
    j["name"] = name;
    j["image"] = image;
    j["tileSize"] = { tileSize.x, tileSize.y };
    j["margin"] = margin;
    j["spacing"] = spacing;
    j["columns"] = columns;
    j["count"] = count;
    if (anchor.x >= 0.f) j["anchor"] = { anchor.x, anchor.y };
    if (scale != 1.f) j["scale"] = scale;
    if (solid) j["solid"] = true;
    j["autotile"] = toString(autotile);
    if (autotile == AutotileMode::Edges16) {
        j["full"] = full;
        j["edge"] = edge;
        j["corner"] = corner;
        if (!ring.empty()) j["ring"] = ring;
        if (!under.empty()) j["under"] = under;
    }
    if (autotile == AutotileMode::Corners16 && frameTime > 0.f) j["frameTime"] = frameTime;
    return j.dump(2) + "\n";
}

bool Tileset::fromJson(const std::string& text, std::string* error) {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { setError(error, "JSON invalide"); return false; }
    if (j.value("format", std::string()) != "angeVault.tileset") { setError(error, "ce n'est pas un tileset AngeVault"); return false; }
    if (j.value("version", 1) > 1) { setError(error, "tileset créé par une version plus récente du moteur"); return false; }

    Tileset t;
    t.file = file;
    t.name = j.value("name", t.name);
    t.image = j.value("image", std::string());
    if (const Json& s = j.value("tileSize", Json()); s.is_array() && s.size() == 2)
        t.tileSize = { std::max(1, s[0].get<int>()), std::max(1, s[1].get<int>()) };
    t.margin = std::max(0, j.value("margin", 0));
    t.spacing = std::max(0, j.value("spacing", 0));
    t.columns = std::max(1, j.value("columns", 1));
    t.count = std::max(0, j.value("count", 0));
    if (const Json& a = j.value("anchor", Json()); a.is_array() && a.size() == 2)
        t.anchor = { a[0].get<float>(), a[1].get<float>() };
    t.scale = j.value("scale", 1.f);
    t.solid = j.value("solid", false);
    t.autotile = autotileFromString(j.value("autotile", std::string("none")));
    t.full = ints(j.value("full", Json()));
    t.edge = four(j.value("edge", Json()));
    t.corner = four(j.value("corner", Json()));
    t.ring = ints(j.value("ring", Json()));
    t.under = ints(j.value("under", Json()));
    t.frameTime = j.value("frameTime", 0.f);
    *this = std::move(t);
    return true;
}

bool loadTileset(Tileset& tileset, const std::string& path, std::string* error) {
    std::ifstream in(toPath(path), std::ios::binary);
    if (!in) { setError(error, "impossible d'ouvrir " + path); return false; }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!tileset.fromJson(ss.str(), error)) { if (error) *error = path + " : " + *error; return false; }
    tileset.missing = false;
    return true;
}

bool saveTileset(Tileset& tileset, const std::string& path, std::string* error) {
    const fs::path target = toPath(path);
    std::error_code ec;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { setError(error, "impossible d'écrire " + path); return false; }
        out << tileset.toJson();
        if (!out) { setError(error, "écriture interrompue : " + path); return false; }
    }
    fs::rename(tmp, target, ec);
    if (ec) { fs::remove(target, ec); fs::rename(tmp, target, ec); }
    if (ec) { setError(error, "impossible de remplacer " + path); return false; }
    tileset.missing = false;
    return true;
}
