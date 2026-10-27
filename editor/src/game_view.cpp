#include "game_view.h"

#include "game_loop.h"
#include "rpg_scene.h"

#include <algorithm>

GameView::GameView(QWidget* parent) : SfmlWidget(parent) {
    onInit = [this](sf::RenderWindow& target) {
        m_game = std::make_unique<Game>(target);   // Game dessine dans ce widget, sans fenêtre à lui
        loadRpgScene(*m_game);                     // projet RPG (projects/rpg/rpg_scene.h)
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

    // La vue est réglée par le CameraSystem du jeu (entité avec Camera2d).
    // Sans caméra dans la scène : vue par défaut, à la taille du widget.
    target.setView(target.getDefaultView());
    m_game->tick(m_paused ? 0.f : dt);   // en pause : on dessine sans faire avancer le jeu
}
