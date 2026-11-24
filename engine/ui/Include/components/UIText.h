#pragma once
#include "ecs/reflect.h"
#include "SFML\System\Vector2.hpp"


struct UIText {
    std::string text;
    std::string defaultText;
    sf::Vector2f offset;
    bool isHidden = false;
};


template<>
inline ReflectedComponent reflectComponent<UIText>(UIText& t) {
    return { "UIText", {
        {"Text", PropertyInfo::Type::String, &t.text },
        {"offset X", PropertyInfo::Type::Vec2f, &t.offset }
    }
    };
}