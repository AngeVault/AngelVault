#pragma once
// paramètres du jeu : fichier assets/settings.cfg du projet (Édition > Paramètres du jeu... dans l'éditeur)
//
//     fullscreen = false
//     vsync = true
//     antialiasing = 4
//     fpsLimit = 0
//     shader = assets/shaders/postprocess.frag
//     shader.contrast = 1.1
//
// fichier absent ou clé manquante : valeurs par défaut
#include <map>
#include <string>

struct GameSettings {
    bool fullscreen = false;
    bool vsync = true;
    unsigned antialiasing = 0;   // 0, 2, 4, 8 ou 16
    unsigned fpsLimit = 0;       // 0 = illimité, ignoré si vsync
    std::string shader;          // post-process .frag, relatif au projet (vide = aucun)
    std::map<std::string, float> shaderParams;   // réglages « @param » du shader

    bool loadFromFile(const std::string& path);   // false si absent / illisible (valeurs inchangées)
    bool saveToFile(const std::string& path) const;
    void loadFromString(const std::string& text);
    std::string toString() const;

    bool operator==(const GameSettings&) const = default;
};
