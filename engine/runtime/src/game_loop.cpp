#include "game_loop.h"
#include "systems/render_system.h"
#include "systems/tilemap_render_system.h"
#include "systems/render_collision_system.h"
#include "systems/force_debug_system.h"
#include "game_systems.h"
#include <iostream>

Game::Game() : Game({ 1920u, 1080u }, "AngeVault") {}

Game::Game(sf::Vector2u size, const std::string& title, const GameSettings& settings)
    : m_size(size), m_title(title), m_settings(settings) {
    m_ownWindow.emplace();
    m_window = &*m_ownWindow;
    createWindow(settings);
    init();
}

Game::Game(sf::RenderWindow& target) : m_window(&target) {
    init();
}

Game::~Game() {
    if (InputManager::current() == &m_input) InputManager::setCurrent(nullptr);
}

void Game::init() {
    InputManager::setCurrent(&m_input);   // Input::getAxis(...) lit ce Game
    m_systems.addSystem<TilemapRenderSystem>(*m_window, m_textureLoader);   // cartes (composant Tilemap), sous les entités
    m_systems.addSystem<RenderSystem>(*m_window, m_textureLoader);
    m_systems.addSystem<RenderCollisionSystem>(*m_window);
    m_systems.addSystem<ForceDebugSystem>(*m_window);
    // Systèmes de jeu du projet (REGISTER_SYSTEM, dossier systems/ du projet)
    for (const GameSystemInfo& info : GameSystemRegistry::get().all()) m_systems.add(info.create(*this, *m_window));
    m_systems.start(m_registry);
}

void Game::applySystemSettings(const std::map<std::string, bool>& enabledByName) {
    for (const auto& [name, enabled] : enabledByName) m_systems.setEnabled(name, enabled);
}

void Game::applySettings(const GameSettings& settings) {
    if (!m_ownWindow) return;   // embarqué : la fenêtre est à l'hôte
    if (settings.fullscreen != m_settings.fullscreen || settings.antialiasing != m_settings.antialiasing)
        createWindow(settings);
    else
        applyFrameRate(settings);
    m_settings = settings;
}

// recrée dans le même objet : m_window et les systèmes gardent leur référence
void Game::createWindow(const GameSettings& settings) {
    sf::ContextSettings context;
    context.antiAliasingLevel = settings.antialiasing;
    if (settings.fullscreen)
        m_ownWindow->create(sf::VideoMode::getDesktopMode(), m_title, sf::State::Fullscreen, context);
    else
        m_ownWindow->create(sf::VideoMode(m_size), m_title, sf::Style::Default, sf::State::Windowed, context);
    applyFrameRate(settings);
}

void Game::applyFrameRate(const GameSettings& settings) {
    // SFML déconseille les deux à la fois : la vsync l'emporte
    m_ownWindow->setVerticalSyncEnabled(settings.vsync);
    m_ownWindow->setFramerateLimit(settings.vsync ? 0 : settings.fpsLimit);
}

void Game::handleEvents() {
    while (const auto event = m_window->pollEvent()) {
        m_input.handleEvent(*event);
        if (event->is<sf::Event::Closed>()) m_isRunning = false;
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            if (key->scancode == sf::Keyboard::Scancode::Escape) m_isRunning = false;
    }
}

void Game::tick(float dt) {
    InputManager::setCurrent(&m_input);
    m_input.update();                 // état des actions pour cette frame
    m_systems.update(m_registry, dt); // tous les systems, dont RenderSystem
    m_registry.flushPendingDestroys();
    m_input.endFrame();               // oublie les « Down / Up » et la molette de cette frame
}

void Game::stop() {
    if (m_stopped) return;
    m_stopped = true;
    m_systems.stop(m_registry);
}

void Game::run() {
    m_clock.restart();
    while (m_isRunning) {
        float dt = m_clock.restart().asSeconds();
        handleEvents();
        m_window->clear(sf::Color::Black);
        tick(dt);
        m_window->display();
    }
    stop();
    std::cout << "[GAME LOOP] Termine.\n";
}
