#pragma once
#include "ecs/reflect.h"


struct UIText {
    std::string text;
    std::string path;
    bool isHidden = false;
    bool isRelativeToCamera = false;
};


template<>
inline ReflectedComponent reflectComponent<UIText>(UIText& t) {
    return { "UIText", {
        {"Text", PropertyInfo::Type::String, &t.text },
        {"isDialog", PropertyInfo::Type::Bool, &t.isRelativeToCamera }
    }
    };
}