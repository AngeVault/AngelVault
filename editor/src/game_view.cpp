#include "game_view.h"

#include "game_loop.h"
#include "qt_sfml_input.h"
#include "rpg_scene.h"
#include "scene/scene_io.h"
#include "scene/tilemaps.h"

#include <QEvent>

#include <algorithm>
#include <iostream>

GameView::GameView(std::string sceneJson, QString inputPath, QString projectDir, MapCopies maps,
                   std::map<std::string, bool> systems, QWidget* parent)
    : SfmlWidget(parent), m_sceneJson(std::move(sceneJson)), m_inputPath(std::move(inputPath)),
      m_projectDir(std::move(projectDir)), m_maps(std::move(maps)), m_systemSettings(std::move(systems)) {
    onInit = [this](sf::RenderWindow& target) {
        m_game = std::make_unique<Game>(target);   // Game dessine dans ce widget, sans fenêtre à lui
        registerRpgSystems(*m_game);               // les systèmes sont du code (projects/rpg/rpg_scene.h)
        m_game->applySystemSettings(m_systemSettings);   // activés / désactivés (.angproj)
        reloadInput();
        SceneMeta meta;                            // les entités viennent de la scène éditée
        const SceneLoadResult scene = sceneFromString(m_game->registry(), meta, m_sceneJson);
        std::vector<std::string> warnings = scene.warnings;
        m_game->registry().view<Tilemap>([this](Entity, Tilemap& tm) {   // cartes de l'éditeur (copies)
            if (auto it = m_maps.find(tm.file); it != m_maps.end()) tm.data = it->second;
        });
        loadTilemaps(m_game->registry(), m_projectDir.toUtf8().toStdString(), &warnings);   // les autres : depuis le disque
        for (const std::string& w : warnings) std::cerr << "[Jeu] " << w << std::endl;
        if (!scene.ok) std::cerr << "[Jeu] Scene illisible : " << scene.error << std::endl;
        m_clock.restart();
    };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);   // position de la souris même sans clic
}

void GameView::reloadInput() {
    if (!m_game) return;
    // Le fichier du projet (modifié par Édition > Input Manager...), pas la copie faite dans bin/ à la compilation
    loadRpgInput(m_game->input(), m_inputPath.toUtf8().toStdString());
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
