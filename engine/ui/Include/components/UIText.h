#pragma once
#include "ecs/reflect.h"


struct UIText {
    std::string text;
    std::string path;
    static UIText create(Registry& registry);
    void Render(Registry& registry, sf::RenderWindow& window);
};


template<>
inline ReflectedComponent reflectComponent<UIText>(UIText& t) {
    return { "UIText", {
        {"Text", PropertyInfo::Type::String, &t.text } }
    };
}