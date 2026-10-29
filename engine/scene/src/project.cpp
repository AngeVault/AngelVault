#include "scene/project.h"
#include "scene/scene_io.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {
// Chemins en UTF-8 (std::string) <-> std::filesystem, y compris les accents sous Windows
fs::path toPath(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }
std::string fromPath(const fs::path& p) {
    const std::u8string s = p.generic_u8string();
    return std::string(s.begin(), s.end());
}
void setError(std::string* error, const std::string& message) { if (error) *error = message; }
} // namespace

bool readFile(const std::string& path, std::string& content, std::string* error) {
    std::ifstream in(toPath(path), std::ios::binary);
    if (!in) { setError(error, "impossible d'ouvrir " + path); return false; }
    std::ostringstream ss;
    ss << in.rdbuf();
    content = ss.str();
    return true;
}

bool writeFileAtomic(const std::string& path, const std::string& content, std::string* error) {
    const fs::path target = toPath(path);
    std::error_code ec;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { setError(error, "impossible d'écrire " + fromPath(tmp)); return false; }
        out << content;
        out.flush();
        if (!out) { setError(error, "écriture interrompue : " + fromPath(tmp)); return false; }
    }
    fs::rename(tmp, target, ec);   // remplace l'ancien fichier d'un coup
    if (ec) {
        // Windows refuse parfois de renommer sur un fichier ouvert ailleurs : on retente en le supprimant
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
        if (ec) { setError(error, "impossible de remplacer " + path + " : " + ec.message()); return false; }
    }
    return true;
}

// ---- Project -----------------------------------------------------------------------------------

bool Project::load(const std::string& path, std::string* error) {
    std::string text;
    if (!readFile(path, text, error)) return false;
    const nlohmann::ordered_json j = nlohmann::ordered_json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { setError(error, path + " : JSON invalide"); return false; }
    if (j.value("format", std::string()) != "angeVault.project") {
        setError(error, path + " : ce n'est pas un projet AngeVault");
        return false;
    }
    if (j.value("version", 1) > kProjectFormatVersion) {
        setError(error, path + " : projet créé par une version plus récente du moteur");
        return false;
    }

    Project p;
    p.name = j.value("name", p.name);
    p.startScene = j.value("startScene", p.startScene);
    if (j.contains("scenes") && j["scenes"].is_array())
        for (const auto& s : j["scenes"]) if (s.is_string()) p.scenes.push_back(s.get<std::string>());
    if (j.contains("paths") && j["paths"].is_object()) {
        p.assetsDir = j["paths"].value("assets", p.assetsDir);
        p.inputPath = j["paths"].value("input", p.inputPath);
    }
    if (j.contains("window") && j["window"].is_object()) {
        const auto& w = j["window"];
        p.window.title = w.value("title", p.name);
        if (w.contains("size") && w["size"].is_array() && w["size"].size() == 2) {
            p.window.width = w["size"][0].get<unsigned>();
            p.window.height = w["size"][1].get<unsigned>();
        }
        p.window.vsync = w.value("vsync", p.window.vsync);
    }
    p.addScene(p.startScene);
    p.filePath = path;
    *this = std::move(p);
    return true;
}

bool Project::save(const std::string& path, std::string* error) {
    if (path.empty()) { setError(error, "aucun chemin pour le projet"); return false; }
    addScene(startScene);
    nlohmann::ordered_json j;
    j["format"] = "angeVault.project";
    j["version"] = kProjectFormatVersion;
    j["name"] = name;
    j["startScene"] = startScene;
    j["scenes"] = scenes;
    j["paths"] = { { "assets", assetsDir }, { "input", inputPath } };
    j["window"] = { { "title", window.title }, { "size", { window.width, window.height } }, { "vsync", window.vsync } };
    if (!writeFileAtomic(path, j.dump(2) + "\n", error)) return false;
    filePath = path;
    return true;
}

std::string Project::directory() const {
    return fromPath(toPath(filePath).parent_path());
}

std::string Project::resolve(const std::string& relative) const {
    const fs::path p = toPath(relative);
    if (p.is_absolute()) return relative;
    return fromPath((toPath(filePath).parent_path() / p).lexically_normal());
}

std::string Project::relative(const std::string& absolute) const {
    const fs::path rel = toPath(absolute).lexically_relative(toPath(filePath).parent_path());
    // En dehors du dossier du projet : on garde le chemin complet
    if (rel.empty() || *rel.begin() == "..") return absolute;
    return fromPath(rel);
}

void Project::addScene(const std::string& relative) {
    if (relative.empty()) return;
    for (const std::string& s : scenes) if (s == relative) return;
    scenes.push_back(relative);
}

bool Project::create(const std::string& directory, const std::string& projectName, Project& out, std::string* error) {
    const fs::path dir = toPath(directory);
    std::error_code ec;
    fs::create_directories(dir / "assets", ec);
    fs::create_directories(dir / "scenes", ec);
    if (ec) { setError(error, "impossible de créer le dossier : " + ec.message()); return false; }

    Project p;
    p.name = projectName;
    p.window.title = projectName;
    p.startScene = "scenes/main.scene";
    p.scenes = { p.startScene };
    const std::string file = fromPath(dir / (projectName + kProjectExtension));
    if (fs::exists(toPath(file))) { setError(error, file + " existe déjà"); return false; }
    if (!p.save(file, error)) return false;

    // Scène de départ vide
    Registry empty;
    SceneMeta meta;
    meta.name = "main";
    if (!saveScene(empty, meta, p.resolve(p.startScene), error)) return false;
    out = std::move(p);
    return true;
}
