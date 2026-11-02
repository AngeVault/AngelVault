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
                // Par index : un comportement peut en ajouter un autre pendant son update
                for (std::size_t i = 0; i < s.behaviors.size(); ++i) {
                    IBehavior* b = s.behaviors[i].behavior.get();
                    if (!b) continue;
                    if (!s.behaviors[i].started) {
                        s.behaviors[i].started = true;
                        b->onStart(e, registry);
                    }
                    b->onUpdate(e, registry, dt);
                }
            });
    }

    void onStop(Registry& registry) override {
        registry.view<Script>(
            [&](Entity e, Script& s) {
                for (Script::Slot& slot : s.behaviors)
                    if (slot.behavior && slot.started) slot.behavior->onDestroy(e, registry);
            });
    }
};