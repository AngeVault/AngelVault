#pragma once
#include "system.h"
#include "registry.h"
#include <algorithm>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
#if defined(__GNUG__)
#include <cxxabi.h>
#include <cstdlib>
#endif

// Nom lisible d'un type C++ : "PhysicsSystem" (sans "class ", ni décoration GCC)
inline std::string prettyTypeName(const std::type_info& type) {
    std::string name = type.name();
#if defined(__GNUG__)
    int status = 0;
    if (char* d = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status)) { name = d; std::free(d); }
#endif
    for (const char* prefix : { "class ", "struct " })
        if (name.rfind(prefix, 0) == 0) name.erase(0, std::char_traits<char>::length(prefix));
    return name;
}

class SystemManager {
public:
    template<typename T, typename... Args>
    T& addSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *sys;
        if (ref.name.empty()) ref.name = prettyTypeName(typeid(T));
        add(std::move(sys));
        return ref;
    }

    // Ajout d'un système déjà construit (systèmes de jeu enregistrés avec REGISTER_SYSTEM)
    ISystem& add(std::unique_ptr<ISystem> sys) {
        ISystem& ref = *sys;
        if (ref.name.empty()) ref.name = prettyTypeName(typeid(ref));
        m_systems.push_back(std::move(sys));
        std::stable_sort(m_systems.begin(), m_systems.end(),
            [](const auto& a, const auto& b) { return a->priority < b->priority; });
        return ref;
    }

    void start(Registry& r) { for (auto& s : m_systems) s->onStart(r); }
    void update(Registry& r, float dt) { for (auto& s : m_systems) if (s->enabled) s->onUpdate(r, dt); }
    void stop(Registry& r) { for (auto& s : m_systems) s->onStop(r); }

    // ---- Activer / désactiver par nom ------------------------------------------------------------
    ISystem* find(const std::string& name) const {
        for (const auto& s : m_systems) if (s->name == name) return s.get();
        return nullptr;
    }
    bool setEnabled(const std::string& name, bool enabled) {   // false si aucun système de ce nom
        ISystem* s = find(name);
        if (s) s->enabled = enabled;
        return s != nullptr;
    }
    bool isEnabled(const std::string& name) const { const ISystem* s = find(name); return s && s->enabled; }

    // Tous les systèmes, dans l'ordre d'exécution (priorité croissante)
    std::vector<ISystem*> list() const {
        std::vector<ISystem*> out;
        for (const auto& s : m_systems) out.push_back(s.get());
        return out;
    }

private:
    std::vector<std::unique_ptr<ISystem>> m_systems;
};
