#pragma once
#include "sfml_widget.h"

#include <SFML/System/Clock.hpp>
#include <memory>

class Game;

// Vue "mode jeu" : fait tourner la scène du RPG dans l'éditeur.
// Les entrées (clavier, souris, molette) passent par l'InputManager du jeu, comme dans rpg.exe.
// Une GameView neuve est créée à chaque lancement (scène remise à zéro) et détruite à l'arrêt.
class GameView : public SfmlWidget {
    Q_OBJECT
public:
    explicit GameView(QWidget* parent = nullptr);
    ~GameView() override;

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }
    void reloadInput();   // relit assets/input.cfg du projet (modifié dans l'Input Manager)

protected:
    bool event(QEvent* e) override;                          // clavier / souris -> InputManager du jeu
    bool focusNextPrevChild(bool) override { return false; } // Tab est une touche du jeu

private:
    void frame(sf::RenderWindow& target);

    std::unique_ptr<Game> m_game;   // détruit avant la fenêtre SFML (membre, donc avant la classe de base)
    sf::Clock m_clock;
    bool m_paused = false;
};
