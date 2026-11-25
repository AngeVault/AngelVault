#pragma once
#include "ecs/reflect.h"


struct UIButton {
    bool isTrigered = false;
    bool triggerEvent = false;
    int with = 70;
    int height = 40;
    void Render(Registry& registry, sf::RenderWindow& window);
};

template<>
inline ReflectedComponent reflectComponent<UIButton>(UIButton& n) {
    return { "UIButton", {
        {"With", PropertyInfo::Type::Int, &n.with },
        {"Height", PropertyInfo::Type::Int, &n.height },
        }
    };
}