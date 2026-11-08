#pragma once
#include <SFML/Graphics.hpp>
#include "ecs/registry.h"
#include "ecs/system_manager.h"
#include "utils/loose_file_loader.h"
#include "input/input_manager.h"
#include "game_settings.h"
#include "render/post_process.h"
#include <map>
#include <optional>
#include <string>

// Deux façons d'utiliser Game :
//  - autonome (Sandbox.exe) : Game game; ... game.run();         -> ouvre sa propre fenêtre
//  - embarqué (éditeur)     : Game game(fenetreExistante); ...
//                             puis game.tick(dt) à chaque frame   -> pas de fenêtre ni de boucle à lui
class Game {
public:
    Game();                                       // crée sa fenêtre 1920x1080
    Game(sf::Vector2u size, const std::string& title, const GameSettings& settings = {});   // fenêtre réglée (.angproj + settings.cfg)
    explicit Game(sf::RenderWindow& target);      // dessine dans une fenêtre fournie (ex. SfmlWidget)
    ~Game();

    void run();             // boucle autonome (événements + clear + tick + display)
    void tick(float dt);    // une seule frame de jeu : systèmes (dont le rendu) + destructions
    void stop();            // appelle onStop des systèmes (une seule fois)

    Registry& registry() { return m_registry; }
    SystemManager& systems() { return m_systems; }
    void setAssetDir(const std::string& dir);   // dossier du projet (textures, shader)

    sf::RenderWindow& window() { return *m_window; }

    // Entrées du jeu (actions nommées). Lues partout avec Input::getAxis("...") (input/input.h).
    // Mode autonome : run() lui passe les événements de la fenêtre.
    // Mode embarqué : c'est à l'hôte (l'éditeur) d'appeler input().handleEvent(...).
    InputManager& input() { return m_input; }

    // Active / désactive des systèmes par leur nom (réglages du projet : "systems" dans le .angproj).
    // Les noms inconnus sont ignorés.
    void applySystemSettings(const std::map<std::string, bool>& enabledByName);

    // Paramètres de assets/settings.cfg. Fenêtre (plein écran, vsync, antialiasing, FPS) : mode autonome
    // seulement, recréée si le plein écran ou l'antialiasing change. Shader rechargé si son chemin change.
    void applySettings(const GameSettings& settings);
    PostProcess& postProcess() { return m_postProcess; }
    const GameSettings& settings() const { return m_settings; }

private:
    std::optional<sf::RenderWindow> m_ownWindow;   // utilisé seulement en mode autonome
    sf::RenderWindow* m_window = nullptr;
    sf::Vector2u     m_size{ 1920u, 1080u };
    std::string      m_title;
    GameSettings     m_settings;
    bool             m_isRunning = true;
    bool             m_stopped = false;
    sf::Clock        m_clock;
    Registry         m_registry;
    SystemManager    m_systems;
    LooseFileLoader  m_textureLoader;
    InputManager     m_input;
    PostProcess      m_postProcess;
    std::string      m_assetDir;


    void init();
    void createWindow(const GameSettings& settings);
    void applyFrameRate(const GameSettings& settings);
    void loadPostProcess();
    void handleEvents();
};
