#include "game_loop.h"
#include "systems/render_system.h"
#include "systems/render_collision_system.h"
#include "systems/force_debug_system.h"
#include <iostream>

Game::Game() {
    m_ownWindow.emplace(sf::VideoMode({ 1920, 1080 }), "AngeVault", sf::Style::Default, sf::State::Windowed);
    m_ownWindow->setVerticalSyncEnabled(true);
    m_window = &*m_ownWindow;
    init();
}

Game::Game(sf::Vector2u size, const std::string& title, bool vsync) {
    m_ownWindow.emplace(sf::VideoMode(size), title, sf::Style::Default, sf::State::Windowed);
    m_ownWindow->setVerticalSyncEnabled(vsync);
    m_window = &*m_ownWindow;
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
    m_systems.addSystem<RenderSystem>(*m_window, m_textureLoader);
    m_systems.addSystem<RenderCollisionSystem>(*m_window);
    m_systems.addSystem<ForceDebugSystem>(*m_window);
    m_systems.start(m_registry);
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
