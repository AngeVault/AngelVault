#pragma once
// Tilemaps d'une scène : le composant Tilemap ne garde dans la scène que le chemin de son fichier .map
// (et « visible »). Ces fonctions chargent / enregistrent les .map de toutes les entités Tilemap.
#include "ecs/registry.h"
#include "tilemap/tilemap.h"

#include <string>
#include <vector>

// Charge les données de chaque Tilemap qui n'en a pas encore (chemins relatifs à projectDir).
// Les messages d'erreur sont ajoutés à `warnings`.
void loadTilemaps(Registry& registry, const std::string& projectDir, std::vector<std::string>* warnings = nullptr);

// Enregistre les .map modifiés (ou tous si `all`). false si au moins un échec.
bool saveTilemaps(Registry& registry, const std::string& projectDir, bool all = false, std::string* error = nullptr);

// Nouvelle carte vide avec des réglages par défaut (orthogonale 32x32, ou isométrique 128x64)
std::shared_ptr<TileMapData> makeTileMap(TileMapOrientation orientation, int width, int height, const std::string& projectDir);
