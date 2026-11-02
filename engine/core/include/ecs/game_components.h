#pragma once
// Composants « de jeu » : déclarés dans le PROJET (projects/rpg/components/), pas dans le moteur.
//
//     // projects/rpg/components/health.h
//     #pragma once
//     #include "ecs/game_components.h"
//
//     struct Health {
//         float hp = 100.f;
//         float maxHp = 100.f;
//         bool invincible = false;
//     };
//     REGISTER_COMPONENT(Health, FIELD(hp), FIELD(maxHp), FIELD(invincible))
//
// Une seule ligne suffit pour que :
//  - l'Inspector le propose dans « Ajouter un composant » et affiche ses champs ;
//  - la scène et les prefabs l'enregistrent ;
//  - on puisse glisser son fichier .h sur une entité.
// Types de champs possibles : float, int, bool, sf::Vector2f, std::string.
// Dans le code du jeu, c'est un composant ECS comme les autres : registry.getComponent<Health>(e).
#include "ecs/reflect.h"
#include "ecs/registry.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

struct GameComponentInfo {
    std::string name;                                         // "Health"
    std::string file;                                         // fichier où il est déclaré (__FILE__)
    std::function<bool(Registry&, Entity)> has;
    std::function<void(Registry&, Entity)> add;
    std::function<void(Registry&, Entity)> remove;
    std::function<ReflectedComponent(Registry&, Entity)> reflect;   // ses champs (pointeurs vers le composant)
};

class GameComponentRegistry {
public:
    static GameComponentRegistry& get() {
        static GameComponentRegistry instance;
        return instance;
    }

    template<typename T>
    bool add(const std::string& name, const std::string& file) {
        GameComponentInfo info;
        info.name = name;
        info.file = file;
        info.has = [](Registry& r, Entity e) { return r.hasComponent<T>(e); };
        info.add = [](Registry& r, Entity e) { if (!r.hasComponent<T>(e)) r.addComponent<T>(e); };
        info.remove = [](Registry& r, Entity e) { r.removeComponent<T>(e); };
        info.reflect = [](Registry& r, Entity e) { return reflectComponent<T>(r.getComponent<T>(e)); };
        for (GameComponentInfo& existing : m_components)
            if (existing.name == name) { existing = std::move(info); return true; }
        m_components.push_back(std::move(info));
        return true;
    }

    const std::vector<GameComponentInfo>& all() const { return m_components; }

    const GameComponentInfo* find(const std::string& name) const {
        for (const GameComponentInfo& c : m_components) if (c.name == name) return &c;
        return nullptr;
    }

    // Composants déclarés dans un fichier (comparé par son nom, ex. "health.h")
    std::vector<const GameComponentInfo*> inFile(const std::string& fileName) const {
        std::vector<const GameComponentInfo*> out;
        for (const GameComponentInfo& c : m_components)
            if (baseName(c.file) == baseName(fileName)) out.push_back(&c);
        return out;
    }

    static std::string baseName(const std::string& path) {
        const auto slash = path.find_last_of("/\\");
        std::string b = slash == std::string::npos ? path : path.substr(slash + 1);
        for (char& ch : b) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        return b;
    }

private:
    std::vector<GameComponentInfo> m_components;
};

// Un champ du composant, dans REGISTER_COMPONENT : FIELD(hp)
#define FIELD(member) \
    PropertyInfo{ #member, PropertyInfo::typeFor<std::decay_t<decltype(avComponent.member)>>(), &avComponent.member }

// Déclare la réflexion du composant ET l'enregistre (à mettre sous la struct, hors de toute fonction)
#define REGISTER_COMPONENT(T, ...)                                                        \
    template<>                                                                            \
    inline ReflectedComponent reflectComponent<T>(T& avComponent) {                       \
        (void)avComponent;                                                                \
        return { #T, { __VA_ARGS__ } };                                                   \
    }                                                                                     \
    inline const bool g_avComponentRegistered_##T = GameComponentRegistry::get().add<T>(#T, __FILE__);
