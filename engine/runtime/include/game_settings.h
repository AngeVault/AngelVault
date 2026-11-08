#pragma once
// Paramètres du jeu : fichier assets/settings.cfg du projet (Édition > Paramètres du jeu... dans l'éditeur).
//
//     fullscreen = false
//     vsync = true
//     antialiasing = 4
//     fpsLimit = 0
//
// Fichier absent ou clé manquante : valeurs par défaut.
#include <string>

struct GameSettings {
    bool fullscreen = false;
    bool vsync = true;
    unsigned antialiasing = 0;   // 0, 2, 4, 8 ou 16
    unsigned fpsLimit = 0;       // 0 = illimité, ignoré si vsync

    bool loadFromFile(const std::string& path);   // false si absent / illisible (valeurs inchangées)
    bool saveToFile(const std::string& path) const;
    void loadFromString(const std::string& text);
    std::string toString() const;

    bool operator==(const GameSettings&) const = default;
};
