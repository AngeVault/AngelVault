#pragma once
#include "ecs/reflect.h"
#include <string>

// point d'arrivée d'un MapTransition
struct SpawnPoint {
    std::string name;
};

template<>
inline ReflectedComponent reflectComponent<SpawnPoint>(SpawnPoint& s) {
    return { "SpawnPoint", { { "name", PropertyInfo::Type::String, &s.name } } };
}
