#pragma once
#include "ecs/reflect.h"


struct UIText {
    std::string text;
    std::string defaultText;
    bool isHidden = false;
};


template<>
inline ReflectedComponent reflectComponent<UIText>(UIText& t) {
    return { "UIText", {
        {"Text", PropertyInfo::Type::String, &t.text }
    }
    };
}