#pragma once
#include "sfml_widget.h"

#include <QString>
#include <SFML/System/Clock.hpp>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

class TileMapData;

class Game;

// Vue "mode jeu" : fait tourner la scène du RPG dans l'éditeur.
// Les entrées (clavier, souris, molette) passent par l'InputManager du jeu, comme dans rpg.exe.
// Une GameView neuve est créée à chaque lancement et détruite à l'arrêt.
// Elle joue une COPIE de la scène de l'éditeur (JSON) : ce qui bouge en jeu ne modifie pas la scène éditée.
class GameView : public SfmlWidget {
    Q_OBJECT
public:
    // sceneJson : la scène à jouer (EditorContext::sceneSnapshot()) ; inputPath : le input.cfg du projet
    // maps : copies des tilemaps de l'éditeur (par fichier .map), pour jouer aussi ce qui n'est pas enregistré
    using MapCopies = std::unordered_map<std::string, std::shared_ptr<TileMapData>>;
    // systems : systèmes activés / désactivés (Project::systems, Exécuter > Systèmes...)
    GameView(std::string sceneJson, QString inputPath, QString projectDir, MapCopies maps,
             std::map<std::string, bool> systems, QWidget* parent = nullptr);
    ~GameView() override;

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }
    Game* game() { return m_game.get(); }   // nullptr tant que la vue n'est pas affichée
    void reloadInput();   // relit le input.cfg du projet (modifié dans l'Input Manager)

protected:
    bool event(QEvent* e) override;                          // clavier / souris -> InputManager du jeu
    bool focusNextPrevChild(bool) override { return false; } // Tab est une touche du jeu

private:
    void frame(sf::RenderWindow& target);

    std::string m_sceneJson;
    QString m_inputPath;
    QString m_projectDir;
    MapCopies m_maps;
    std::map<std::string, bool> m_systemSettings;
    std::unique_ptr<Game> m_game;   // détruit avant la fenêtre SFML (membre, donc avant la classe de base)
    sf::Clock m_clock;
    bool m_paused = false;
};
