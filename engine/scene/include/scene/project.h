#pragma once
// Fichier projet .angproj (JSON) : décrit le projet, pas son contenu.
//
//     {
//       "format": "angeVault.project", "version": 1,
//       "name": "RPG",
//       "startScene": "scenes/main.scene",
//       "scenes": [ "scenes/main.scene" ],
//       "paths": { "assets": "assets", "input": "assets/input.cfg" },
//       "window": { "title": "RPG", "size": [1920, 1080], "vsync": true }
//     }
//
// Tous les chemins sont relatifs au dossier du .angproj : resolve() donne le chemin complet.
#include <cstdint>
#include <string>
#include <vector>

inline constexpr int kProjectFormatVersion = 1;
inline constexpr const char* kProjectExtension = ".angproj";
inline constexpr const char* kSceneExtension = ".scene";

struct ProjectWindow {
    std::string title = "AngeVault";
    unsigned width = 1920;
    unsigned height = 1080;
    bool vsync = true;
};

struct Project {
    std::string name = "Projet";
    std::string startScene = "scenes/main.scene";
    std::vector<std::string> scenes;
    std::string assetsDir = "assets";
    std::string inputPath = "assets/input.cfg";
    ProjectWindow window;

    std::string filePath;   // chemin du .angproj (rempli par load / save), pas écrit dans le fichier

    bool load(const std::string& path, std::string* error = nullptr);
    bool save(const std::string& path, std::string* error = nullptr);   // met à jour filePath
    bool save(std::string* error = nullptr) { return save(filePath, error); }

    std::string directory() const;                          // dossier du .angproj
    std::string resolve(const std::string& relative) const; // chemin relatif au projet -> chemin complet
    std::string relative(const std::string& absolute) const;// inverse (pour enregistrer un chemin choisi)
    void addScene(const std::string& relative);             // sans doublon

    // Crée un nouveau projet : dossier/nom.angproj, assets/, scenes/ et une scène vide.
    static bool create(const std::string& directory, const std::string& name, Project& out, std::string* error = nullptr);
};

// Écrit un fichier sans risque de le laisser à moitié écrit (fichier .tmp puis renommage)
bool writeFileAtomic(const std::string& path, const std::string& content, std::string* error = nullptr);
bool readFile(const std::string& path, std::string& content, std::string* error = nullptr);
