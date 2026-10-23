#pragma once
#include "system.h"
#include "registry.h"
#include <vector>
#include <memory>
#include <algorithm>

class SystemManager {
public:
    template<typename T, typename... Args>
    T& addSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *sys;
        m_systems.push_back(std::move(sys));
        std::sort(m_systems.begin(), m_systems.end(),
            [](const auto& a, const auto& b) { return a->priority < b->priority; });
        return ref;
    }

    void start(Registry& r) { for (auto& s : m_systems) s->onStart(r); }
    void update(Registry& r, float dt) { for (auto& s : m_systems) s->onUpdate(r, dt); }
    void stop(Registry& r) { for (auto& s : m_systems) s->onStop(r); }

private:
    std::vector<std::unique_ptr<ISystem>> m_systems;
};