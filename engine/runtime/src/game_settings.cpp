#include "game_settings.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace {
// indépendant de la locale : qt met le séparateur décimal en virgule et std::stof lit alors "0.9" comme 0
bool toFloat(const std::string& s, float& out) {
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && end != s.data();
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

bool toBool(const std::string& v, bool fallback) {
    if (v == "true" || v == "1") return true;
    if (v == "false" || v == "0") return false;
    return fallback;
}

unsigned toUnsigned(const std::string& v, unsigned fallback) {
    try { return unsigned(std::max(0, std::stoi(v))); } catch (...) { return fallback; }
}
} // namespace

bool GameSettings::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    loadFromString(ss.str());
    return true;
}

bool GameSettings::saveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << toString();
    return bool(out);
}

void GameSettings::loadFromString(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "fullscreen") fullscreen = toBool(value, fullscreen);
        else if (key == "vsync") vsync = toBool(value, vsync);
        else if (key == "antialiasing") antialiasing = std::min(toUnsigned(value, antialiasing), 16u);
        else if (key == "fpsLimit") fpsLimit = toUnsigned(value, fpsLimit);
        else if (key == "shader") shader = value;
        else if (key.rfind("shader.", 0) == 0) {
            if (float v; toFloat(value, v)) shaderParams[key.substr(7)] = v;
        }
    }
}

std::string GameSettings::toString() const {
    std::ostringstream out;
    out << "# AngeVault : parametres du jeu (Edition > Parametres du jeu... dans l'editeur)\n"
        << "fullscreen = " << (fullscreen ? "true" : "false") << "\n"
        << "vsync = " << (vsync ? "true" : "false") << "\n"
        << "antialiasing = " << antialiasing << "\n"
        << "fpsLimit = " << fpsLimit << "\n"
        << "shader = " << shader << "\n";
    for (const auto& [name, value] : shaderParams) out << "shader." << name << " = " << value << "\n";
    return out.str();
}
