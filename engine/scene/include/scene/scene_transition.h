#pragma once
// changement de map : garde les DontDestroy et place le joueur sur le SpawnPoint
#include "ecs/registry.h"

#include <string>
#include <vector>

struct SceneChangeResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> warnings;
};

// scenePath et spawnName sont relatifs au projet (comme Tilemap::file)
SceneChangeResult changeScene(Registry& registry, const std::string& projectDir,
                              const std::string& scenePath, const std::string& spawnName);
