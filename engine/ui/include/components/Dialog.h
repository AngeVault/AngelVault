#pragma once
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include "game_loop.h"
#include "systems/collision_system.h"
#include "components/geometry.h"
#include "components/UIText.h"



struct Dialog
{
    std::vector<std::string> dialogs;
    std::string filePath;
    int currentLine = -1;
    int currentChar = 0;
    float textSpeed = 0.04f;
    float textTimer = 0.f;
};

template<>
inline ReflectedComponent reflectComponent<Dialog>(Dialog& d) {
    return { "Dialog", {
            {"json", PropertyInfo::Type::String, &d.filePath },
            {"json", PropertyInfo::Type::Float, &d.textSpeed }
    }
    };
}