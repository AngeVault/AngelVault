#pragma once
#include "sfml_widget.h"

#include <SFML/System/Clock.hpp>
#include <memory>

class Game;

// Vue "mode jeu" : fait tourner la scène du Sandbox dans l'éditeur.
// Une GameView neuve est créée à chaque lancement (scène remise à zéro) et détruite à l'arrêt.
class GameView : public SfmlWidget {
    Q_OBJECT
public:
    explicit GameView(QWidget* parent = nullptr);
    ~GameView() override;

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }

private:
    void frame(sf::RenderWindow& target);

    std::unique_ptr<Game> m_game;   // détruit avant la fenêtre SFML (membre, donc avant la classe de base)
    sf::Clock m_clock;
    bool m_paused = false;
};
