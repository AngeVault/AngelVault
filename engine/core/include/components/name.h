#pragma once
#include <string>
#include "ecs/reflect.h"


struct NameComponent {
    std::string name;

    explicit NameComponent(std::string n = "") : name(std::move(n)) {}
};

template<>
inline ReflectedComponent reflectComponent<NameComponent>(NameComponent& n) {
    return { "NameComponent", 
        { { "name", PropertyInfo::Type::String, &n.name }, } 
    };
}