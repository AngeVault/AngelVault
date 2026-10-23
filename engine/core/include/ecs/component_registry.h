#pragma once
#include "reflect.h"
#include "registry.h"
#include <functional>
#include <unordered_map>
#include <typeindex>

// Fonction qui prend un Registry + Entity et retourne un ReflectedComponent
using ReflectorFn = std::function<ReflectedComponent(Registry&, Entity)>;

class ComponentRegistry {
public:
    static ComponentRegistry& get() {
        static ComponentRegistry instance;
        return instance;
    }

    // Enregistre un type de composant avec sa fonction de réflexion
    template<typename T>
    void registerComponent() {
        m_reflectors[typeid(T)] = [](Registry& reg, Entity e) -> ReflectedComponent {
            if (!reg.hasComponent<T>(e)) return {};
            return reflectComponent<T>(reg.getComponent<T>(e));
            };
    }

    // Retourne tous les composants réfléchis d'une entité
    std::vector<ReflectedComponent> reflectEntity(Registry& reg, Entity e) {
        std::vector<ReflectedComponent> result;
        for (auto& [type, fn] : m_reflectors) {
            auto reflected = fn(reg, e);
            if (!reflected.name.empty())
                result.push_back(std::move(reflected));
        }
        return result;
    }

private:
    std::unordered_map<std::type_index, ReflectorFn> m_reflectors;
};