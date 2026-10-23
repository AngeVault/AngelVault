#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/script_component.h"

class ScriptSystem : public ISystem {
public:
    ScriptSystem() { priority = 2; } // après Physics (0), après collision (1),  avant Render (10)

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Script>(
            [&](Entity e, Script& s) {
                if (!s.behavior) return;

                if (!s.started) {
                    s.behavior->onStart(e, registry);
                    s.started = true;
                }

                s.behavior->onUpdate(e, registry, dt);
            });
    }

    void onStop(Registry& registry) override {
        registry.view<Script>(
            [&](Entity e, Script& s) {
                if (s.behavior && s.started)
                    s.behavior->onDestroy(e, registry);
            });
    }
};