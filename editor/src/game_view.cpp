#include "game_view.h"

#include "game_loop.h"
#include "sandbox_scene.h"

#include <algorithm>

namespace {
// Montre tout le monde du jeu en gardant ses proportions (bandes sur les côtés si besoin)
sf::View fitView(sf::Vector2u windowSize, sf::Vector2f worldSize) {
    sf::View view(sf::FloatRect({ 0.f, 0.f }, worldSize));
    const float windowRatio = float(windowSize.x) / float(std::max(1u, windowSize.y));
    const float worldRatio = worldSize.x / worldSize.y;
    if (windowRatio > worldRatio) {
        const float w = worldRatio / windowRatio;
        view.setViewport(sf::FloatRect({ (1.f - w) / 2.f, 0.f }, { w, 1.f }));
    } else {
        const float h = windowRatio / worldRatio;
        view.setViewport(sf::FloatRect({ 0.f, (1.f - h) / 2.f }, { 1.f, h }));
    }
    return view;
}
}

GameView::GameView(QWidget* parent) : SfmlWidget(parent) {
    onInit = [this](sf::RenderWindow& target) {
        m_game = std::make_unique<Game>(target);   // Game dessine dans ce widget, sans fenêtre à lui
        loadSandboxScene(*m_game);
        m_clock.restart();
    };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };
}

GameView::~GameView() {
    if (m_game) m_game->stop();
    m_game.reset();
}

void GameView::frame(sf::RenderWindow& target) {
    if (!m_game) return;
    // dt limité : évite un saut de physique après un blocage (déplacement de fenêtre, menu...)
    const float dt = std::min(m_clock.restart().asSeconds(), 0.05f);

    target.setView(fitView(target.getSize(), { kSandboxWorldWidth, kSandboxWorldHeight }));
    sf::RectangleShape background({ kSandboxWorldWidth, kSandboxWorldHeight });
    background.setFillColor(sf::Color::Black);
    target.draw(background);

    m_game->tick(m_paused ? 0.f : dt);   // en pause : on dessine sans faire avancer le jeu
}
