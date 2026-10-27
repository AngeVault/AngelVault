#include "game_view.h"

#include "game_loop.h"
#include "qt_sfml_input.h"
#include "rpg_scene.h"

#include <QEvent>

#include <algorithm>

GameView::GameView(QWidget* parent) : SfmlWidget(parent) {
    onInit = [this](sf::RenderWindow& target) {
        m_game = std::make_unique<Game>(target);   // Game dessine dans ce widget, sans fenêtre à lui
        loadRpgScene(*m_game);                     // projet RPG (projects/rpg/rpg_scene.h)
        reloadInput();
        m_clock.restart();
    };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);   // position de la souris même sans clic
}

void GameView::reloadInput() {
    if (!m_game) return;
#ifdef AV_PROJECT_DIR
    // Le fichier du projet (modifié par Édition > Input Manager...), pas la copie faite dans bin/ à la compilation
    loadRpgInput(m_game->input(), AV_PROJECT_DIR "/assets/input.cfg");
#endif
}

bool GameView::event(QEvent* e) {
    if (m_game) {
        if (const std::optional<sf::Event> ev = QtSfml::toSfmlEvent(e, devicePixelRatioF())) {
            m_game->input().handleEvent(*ev);
            if (e->type() == QEvent::MouseButtonPress) setFocus();   // cliquer dans le jeu lui rend le clavier
        }
    }
    return SfmlWidget::event(e);
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
