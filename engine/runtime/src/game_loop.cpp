#include "game_loop.h"
#include "systems/render_system.h"
#include "systems/render_collision_system.h"
#include "systems/force_debug_system.h"
#include <iostream>

Game::Game() : m_isRunning(true) {
    m_window = sf::RenderWindow(sf::VideoMode({ 1920, 1080 }), "AngeVault", sf::Style::Default, sf::State::Windowed);
    m_window.setVerticalSyncEnabled(true);
    
    m_systems.addSystem<RenderSystem>(m_window, m_textureLoader);
    m_systems.addSystem<RenderCollisionSystem>(m_window);
    m_systems.addSystem<ForceDebugSystem>(m_window);
    m_systems.start(m_registry);
}

void Game::handleEvents() {
    while (const auto event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) m_isRunning = false;
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            if (key->scancode == sf::Keyboard::Scancode::Escape) m_isRunning = false;
    }
}

void Game::update(float dt) {
    m_systems.update(m_registry, dt); // tous les systems, dont RenderSystem
    m_registry.flushPendingDestroys();
}

void Game::render() {
    m_window.display();
}


void Game::run() {
    m_clock.restart();
    while (m_isRunning) {
        float dt = m_clock.restart().asSeconds();
        handleEvents();
        m_window.clear(sf::Color::Black); 
        update(dt);
        render();                     
    }
    m_systems.stop(m_registry);
    std::cout << "[GAME LOOP] Termine.\n";
}