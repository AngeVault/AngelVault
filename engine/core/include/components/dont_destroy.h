#pragma once
#include "ecs/reflect.h"
#include <string>

// gardée d'une scène à l'autre, une par clé
struct DontDestroy {
    std::string key;   // "Player", "Camera"...
};

template<>
inline ReflectedComponent reflectComponent<DontDestroy>(DontDestroy& d) {
    return { "DontDestroy", { { "key", PropertyInfo::Type::String, &d.key } } };
}
