#pragma once
#include "sfml_widget.h"

#include "RtsTile_map.hpp"
#include "components/camera2d.h"
#include "ecs/registry.h"
#include "utils/loose_file_loader.h"

#include <QPoint>
#include <SFML/System/Clock.hpp>
#include <memory>

class TileMapRenderSystem;

// Vue d'édition : affiche une TileMap avec le TileMapRenderSystem du moteur.
//  - molette        : zoom (centré sur la souris)
//  - clic molette / clic droit + glisser : déplacer la vue
//  - F (ou recentrer()) : cadrer toute la carte
class TilemapView : public SfmlWidget {
    Q_OBJECT
public:
    explicit TilemapView(QWidget* parent = nullptr);
    ~TilemapView() override;

    TileMap& map() { return m_map; }
    Camera2d& camera() { return m_camera; }

    void setGridVisible(bool visible) { m_showGrid = visible; }
    bool isGridVisible() const { return m_showGrid; }
    void fitToMap();

signals:
    void hoveredTileChanged(int x, int y, const QString& type);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void init(sf::RenderWindow& target);
    void frame(sf::RenderWindow& target);
    void buildDemoMap();
    void buildTileset();
    void drawGrid(sf::RenderTarget& target);
    sf::Vector2f toWorld(const QPointF& widgetPos);

    TileMap m_map;
    Camera2d m_camera;
    LooseFileLoader m_textures{ "" };          // charge les PNG des tilesets (id = chemin)
    Registry m_registry;                       // requis par l'interface ISystem
    std::unique_ptr<TileMapRenderSystem> m_renderer;
    sf::Clock m_clock;

    bool m_showGrid = true;
    bool m_needsFit = true;
    bool m_panning = false;
    QPointF m_lastMouse;
    sf::Vector2i m_hovered{ -1, -1 };
};
