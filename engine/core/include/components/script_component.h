#pragma once
// Comportements (scripts) d'une entité. Comme dans Unity, une entité peut en avoir PLUSIEURS :
//
//     auto& script = registry.addComponent<Script>(player);
//     script.add<PlayerBehavior>();
//     script.add<Rotation>().speed = 90.f;
//
//     if (auto* p = registry.getComponent<Script>(player).get<PlayerBehavior>()) p->speed = 300.f;
//
// Le ScriptSystem appelle onStart (une fois) puis onUpdate de chacun, dans l'ordre.
#include "ecs/behavior.h"

#include <cstddef>
#include <memory>
#include <vector>

struct Script {
    struct Slot {
        std::unique_ptr<IBehavior> behavior;
        bool started = false;   // onStart déjà appelé
    };
    std::vector<Slot> behaviors;

    Script() = default;
    explicit Script(std::unique_ptr<IBehavior> b) { add(std::move(b)); }

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    Script(Script&&) = default;
    Script& operator=(Script&&) = default;

    IBehavior* add(std::unique_ptr<IBehavior> b) {
        if (!b) return nullptr;
        behaviors.push_back({ std::move(b), false });
        return behaviors.back().behavior.get();
    }

    template<typename T, typename... Args>
    T& add(Args&&... args) {
        return static_cast<T&>(*add(std::make_unique<T>(std::forward<Args>(args)...)));
    }

    // Premier comportement de type T (nullptr si absent)
    template<typename T>
    T* get() const {
        for (const Slot& s : behaviors)
            if (auto* t = dynamic_cast<T*>(s.behavior.get())) return t;
        return nullptr;
    }

    void remove(std::size_t index) {
        if (index < behaviors.size()) behaviors.erase(behaviors.begin() + std::ptrdiff_t(index));
    }

    std::size_t size() const { return behaviors.size(); }
    bool empty() const { return behaviors.empty(); }
};
