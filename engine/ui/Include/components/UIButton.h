#pragma once
#include "ecs/reflect.h"


struct UIButton {
    bool isTrigered = false;
    int r = sf::Color::Green.r;
    int g = sf::Color::Green.g;
    int b = sf::Color::Green.b;
    int with = 70;
    int height = 40;
    void Render(Registry& registry, sf::RenderWindow& window);
};

template<>
inline ReflectedComponent reflectComponent<UIButton>(UIButton& n) {
    return { "UIButton", {
        {"Red", PropertyInfo::Type::Int, &n.r },
        {"Green", PropertyInfo::Type::Int, &n.g },
        {"Blue", PropertyInfo::Type::Int, &n.b },
        }
    };
}