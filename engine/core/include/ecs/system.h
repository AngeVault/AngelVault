#pragma once

class Registry; 

struct ISystem {
    virtual ~ISystem() = default;
    virtual void onStart(Registry& registry) {}
    virtual void onUpdate(Registry& registry, float dt) = 0;
    virtual void onStop(Registry& registry) {}

    int priority = 0;
};