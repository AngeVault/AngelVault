#pragma once
#include "ecs/reflect.h"
#include <string>

// change de map au contact du joueur (Collider2D en trigger)
struct MapTransition {
    std::string scene;   // fichier .scene cible, relatif au projet
    std::string spawn;   // nom du SpawnPoint visé dans cette scène
};

template<>
inline ReflectedComponent reflectComponent<MapTransition>(MapTransition& m) {
    return { "MapTransition", {
        { "scene", PropertyInfo::Type::String, &m.scene },
        { "spawn", PropertyInfo::Type::String, &m.spawn },
    } };
}
