#pragma once
#include <SFML/Graphics.hpp>
#include "ecs/registry.h"
#include "ecs/system_manager.h"
#include "utils/loose_file_loader.h"
#include <optional>

// Deux façons d'utiliser Game :
//  - autonome (Sandbox.exe) : Game game; ... game.run();         -> ouvre sa propre fenêtre
//  - embarqué (éditeur)     : Game game(fenetreExistante); ...
//                             puis game.tick(dt) à chaque frame   -> pas de fenêtre ni de boucle à lui
class Game {
public:
    Game();                                       // crée sa fenêtre 1920x1080
    explicit Game(sf::RenderWindow& target);      // dessine dans une fenêtre fournie (ex. SfmlWidget)
    ~Game() = default;

    void run();             // boucle autonome (événements + clear + tick + display)
    void tick(float dt);    // une seule frame de jeu : systèmes (dont le rendu) + destructions
    void stop();            // appelle onStop des systèmes (une seule fois)

    Registry& registry() { return m_registry; }
    SystemManager& systems() { return m_systems; }

    sf::RenderWindow& window() { return *m_window; }

private:
    std::optional<sf::RenderWindow> m_ownWindow;   // utilisé seulement en mode autonome
    sf::RenderWindow* m_window = nullptr;
    bool             m_isRunning = true;
    bool             m_stopped = false;
    sf::Clock        m_clock;
    Registry         m_registry;
    SystemManager    m_systems;
    LooseFileLoader  m_textureLoader;


    void init();
    void handleEvents();
};
