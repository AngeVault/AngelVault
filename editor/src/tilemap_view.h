#pragma once
#include "sfml_widget.h"
#include "editor_context.h"

#include "components/camera2d.h"
#include "utils/loose_file_loader.h"

#include <QPoint>
#include <SFML/System/Clock.hpp>
#include <memory>

class TileMapRenderSystem;
class RenderSystem;

// Vue scène de l'éditeur : la tilemap + les entités de la scène (EditorContext).
//  - molette : zoom (centré sur la souris)     - clic droit / clic molette + glisser : déplacer la vue
//  - outil Sélection : clic = sélectionner, glisser = déplacer l'entité (Ctrl = aligner sur la grille)
//  - outils Pinceau / Gomme / Pipette : voir la Tile Palette
//  - F : cadrer la carte   Suppr : supprimer l'entité   Échap : désélectionner
//
// La caméra de cette vue est celle de l'ÉDITEUR : elle ne fait pas partie de la scène.
class TilemapView : public SfmlWidget {
    Q_OBJECT
public:
    explicit TilemapView(EditorContext& context, QWidget* parent = nullptr);
    ~TilemapView() override;

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
    void resizeEvent(QResizeEvent* e) override;

private:
    void init(sf::RenderWindow& target);
    void frame(sf::RenderWindow& target);
    void buildDemoMap();
    void buildTileset();
    void drawGrid(sf::RenderTarget& target);
    void drawSelection(sf::RenderTarget& target);
    void drawBrush(sf::RenderTarget& target);
    void updateCursor();

    sf::Vector2f toWorld(const QPointF& widgetPos);
    Entity pickEntity(sf::Vector2f world);
    void paintAt(sf::Vector2i tile);
    void updateHover(const QPointF& pos);

    EditorContext& m_ctx;
    Camera2d m_camera;
    LooseFileLoader m_textures{ "" };          // charge les PNG (id = chemin)
    std::unique_ptr<TileMapRenderSystem> m_tileRenderer;
    std::unique_ptr<RenderSystem> m_entityRenderer;
    sf::Clock m_clock;

    bool m_showGrid = true;
    bool m_needsFit = true;
    bool m_userCamera = false;   // la caméra a été bougée à la main : plus de recadrage auto
    bool m_panning = false;
    bool m_painting = false;
    QPointF m_lastMouse;

    Entity m_dragEntity = NULL_ENTITY;
    sf::Vector2f m_dragOffset;

    sf::Vector2i m_hoveredTile{ 0, 0 };
    bool m_hoverValid = false;
    sf::Vector2i m_lastPainted{ INT32_MIN, INT32_MIN };
};
