#pragma once
#include "ecs/reflect.h"


struct UIRect {
    static UIRect create(Registry& registry);
    void Render(Registry& registry, sf::RenderWindow& window);
};

template<>
inline ReflectedComponent reflectComponent<UIRect>(UIRect& n) {
    return { "UIRect",
        { { }, }
    };
}