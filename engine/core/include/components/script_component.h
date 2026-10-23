#pragma once
#include "ecs/behavior.h"
#include <memory>

struct Script {
    std::unique_ptr<IBehavior> behavior;
    bool started = false;

    Script() = default;

    explicit Script(std::unique_ptr<IBehavior> b)
        : behavior(std::move(b)) {
    }

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    Script(Script&&) = default;
    Script& operator=(Script&&) = default;
};