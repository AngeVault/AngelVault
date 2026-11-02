#pragma once
// Systèmes « de jeu » : déclarés dans le PROJET (projects/rpg/systems/), pas dans le moteur.
//
//     // projects/rpg/systems/spin_system.h
//     #pragma once
//     #include "game_systems.h"
//     #include "components/spin.h"
//
//     class SpinSystem : public ISystem {
//     public:
//         SpinSystem() { priority = 3; }   // ordre d'exécution (Render = 10 : dessiner en dernier)
//         void onUpdate(Registry& registry, float dt) override {
//             registry.view<Spin, Transform2D>([&](Entity, Spin& s, Transform2D& t) { ... });
//         }
//     };
//     REGISTER_SYSTEM(SpinSystem)
//
// Tout système enregistré est ajouté automatiquement à chaque Game (rpg.exe, et F5 dans l'éditeur).
// Son constructeur peut être vide, ou prendre Game& ou sf::RenderWindow& (pour dessiner).
// On l'active / désactive comme les systèmes du moteur : éditeur > Exécuter > Systèmes...
#include "ecs/registry.h"
#include "ecs/system.h"
#include "ecs/system_manager.h"

#include <SFML/Graphics/RenderWindow.hpp>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

class Game;

struct GameSystemInfo {
    std::string name;
    std::string file;   // __FILE__ de REGISTER_SYSTEM
    std::function<std::unique_ptr<ISystem>(Game& game, sf::RenderWindow& window)> create;
};

class GameSystemRegistry {
public:
    static GameSystemRegistry& get() {
        static GameSystemRegistry instance;
        return instance;
    }

    template<typename T>
    bool add(const std::string& name, const std::string& file) {
        GameSystemInfo info;
        info.name = name;
        info.file = file;
        info.create = [name](Game& game, sf::RenderWindow& window) -> std::unique_ptr<ISystem> {
            std::unique_ptr<ISystem> s;
            if constexpr (std::is_constructible_v<T, Game&>) s = std::make_unique<T>(game);
            else if constexpr (std::is_constructible_v<T, sf::RenderWindow&>) s = std::make_unique<T>(window);
            else s = std::make_unique<T>();
            (void)game; (void)window;
            s->name = name;
            return s;
        };
        for (GameSystemInfo& existing : m_systems)
            if (existing.name == name) { existing = std::move(info); return true; }
        m_systems.push_back(std::move(info));
        return true;
    }

    const std::vector<GameSystemInfo>& all() const { return m_systems; }

private:
    std::vector<GameSystemInfo> m_systems;
};

#define REGISTER_SYSTEM(T) \
    inline const bool g_avSystemRegistered_##T = GameSystemRegistry::get().add<T>(#T, __FILE__);
