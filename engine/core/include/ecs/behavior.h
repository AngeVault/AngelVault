#pragma once
#include "entity.h"
#include "property.h"
#include <vector>

class Registry;

struct IBehavior {
    std::vector<PropertyInfo> m_properties;

    virtual ~IBehavior() = default;

    virtual void onStart(Entity self, Registry& registry) {}
    virtual void onUpdate(Entity self, Registry& registry, float dt) = 0;
    virtual void onDestroy(Entity self, Registry& registry) {}
};