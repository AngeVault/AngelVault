#include "game_loop.h"
#include "systems/render_system.h"
#include "systems/tilemap_render_system.h"
#include "systems/day_night_system.h"
#include "systems/render_collision_system.h"
#include "systems/force_debug_system.h"
#include "game_systems.h"
#include <algorithm>
#include <filesystem>
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
    syncScene();                          // taille valide dès onStart
    m_systems.addSystem<TilemapRenderSystem>(*m_window, m_textureLoader);   // cartes (composant Tilemap), sous les entités
    m_systems.addSystem<RenderSystem>(*m_window, m_textureLoader);
    m_systems.addSystem<DayNightSystem>(*m_window);   // composant DayNightCycle : heure + teinte de l'écran
    m_systems.addSystem<RenderCollisionSystem>(*m_window);
    m_systems.addSystem<ForceDebugSystem>(*m_window);
    // Systèmes de jeu du projet (REGISTER_SYSTEM, dossier systems/ du projet)
    for (const GameSystemInfo& info : GameSystemRegistry::get().all()) m_systems.add(info.create(*this, m_scene));
    m_systems.start(m_registry);
}

void Game::applySystemSettings(const std::map<std::string, bool>& enabledByName) {
    for (const auto& [name, enabled] : enabledByName) m_systems.setEnabled(name, enabled);
}

void Game::setAssetDir(const std::string& dir) {
    m_assetDir = dir;
    m_textureLoader.setBaseDir(dir);
    loadPostProcess();   // le chemin du shader est relatif au projet
}

void Game::applySettings(const GameSettings& settings) {
    if (m_ownWindow) {   // embarqué : la fenêtre est à l'hôte
        if (settings.fullscreen != m_settings.fullscreen)
            createWindow(settings);
        else
            applyFrameRate(settings);
    }
    const bool newShader = settings.shader != m_settings.shader;
    m_settings = settings;
    if (newShader) loadPostProcess();
    else m_postProcess.setParams(m_settings.shaderParams);   // curseurs de l'éditeur : pas de recompilation
}

void Game::loadPostProcess() {
    namespace fs = std::filesystem;
    auto u8 = [](const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); };
    fs::path path = u8(m_settings.shader);
    if (!path.empty() && path.is_relative()) path = u8(m_assetDir) / path;
    const std::u8string full = path.lexically_normal().generic_u8string();

    std::string error;
    if (!m_postProcess.load(std::string(full.begin(), full.end()), &error)) std::cerr << "[Post-process] " << error << "\n";
    m_postProcess.setParams(m_settings.shaderParams);
}

// recrée dans le même objet : m_window garde son adresse
// pas d'antialiasing ici, il est sur m_scene (la fenêtre ne reçoit qu'une image déjà lissée)
void Game::createWindow(const GameSettings& settings) {
    if (settings.fullscreen)
        m_ownWindow->create(sf::VideoMode::getDesktopMode(), m_title, sf::State::Fullscreen);
    else
        m_ownWindow->create(sf::VideoMode(m_size), m_title, sf::Style::Default, sf::State::Windowed);
    applyFrameRate(settings);
}

// l'image du jeu suit la taille de la fenêtre (widget redimensionné, plein écran) et l'antialiasing
// resize() garde le même objet : les systèmes gardent leur référence
void Game::syncScene() {
    const sf::Vector2u size = m_window->getSize();
    const unsigned aa = std::min(m_settings.antialiasing, sf::RenderTexture::getMaximumAntiAliasingLevel());
    if (m_scene.getSize() == size && m_sceneAA == aa) return;
    sf::ContextSettings context;
    context.antiAliasingLevel = aa;
    if (!m_scene.resize(size, context)) std::cerr << "[Game] impossible de créer l'image du jeu\n";
    m_sceneAA = aa;
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
    syncScene();
    m_scene.clear(sf::Color::Black);
    m_systems.update(m_registry, dt); // tous les systems, dont RenderSystem
    m_scene.display();
    m_postProcess.draw(m_scene.getTexture(), *m_window);   // à l'écran, à travers le shader s'il y en a un
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
