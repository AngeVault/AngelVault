#pragma once
#include "sfml_widget.h"
#include "editor_context.h"

#include "utils/loose_file_loader.h"

#include <QPoint>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Clock.hpp>
#include <memory>

class TilemapRenderSystem;

// Caméra de l'ÉDITEUR (pan / zoom de la vue scène). Ce n'est pas une entité :
// elle ne fait pas partie de la scène et n'est jamais sauvegardée avec elle.
struct EditorCamera {
    sf::Vector2f position{ 0.f, 0.f };   // centre de la vue, en unités monde
    float zoom = 1.f;
    float minZoom = 0.05f;
    float maxZoom = 8.f;

    sf::View makeView(sf::Vector2u windowSize) const {
        return sf::View(position, { windowSize.x / zoom, windowSize.y / zoom });
    }
};
class RenderSystem;

// Vue scène de l'éditeur : la tilemap + les entités de la scène (EditorContext).
//  - molette : zoom (centré sur la souris)     - clic droit / clic molette + glisser : déplacer la vue
//  - outil Sélection : clic = sélectionner, glisser = déplacer l'entité (Ctrl = aligner sur la grille)
//  - outils Pinceau / Gomme / Remplir / Pipette : voir la Tile Palette
//      glisser = tracé continu (aucune case sautée)   Maj + clic = ligne droite depuis le dernier point
//      Ctrl + molette, [ ] ou - + : taille du pinceau   Alt + clic : pipette
//  - F : cadrer la carte   Suppr : supprimer l'entité   Échap : désélectionner
//
// La caméra de cette vue est celle de l'ÉDITEUR : elle ne fait pas partie de la scène.
class TilemapView : public SfmlWidget {
    Q_OBJECT
public:
    explicit TilemapView(EditorContext& context, QWidget* parent = nullptr);
    ~TilemapView() override;

    EditorCamera& camera() { return m_camera; }

    void setGridVisible(bool visible) { m_showGrid = visible; }
    bool isGridVisible() const { return m_showGrid; }
    void fitToMap();

signals:
    void hoveredTileChanged(int x, int y, const QString& type);
    void statusText(const QString& text);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;   // prefab (.prefab) ou comportement (.h) du panneau Project
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void init(sf::RenderWindow& target);
    void frame(sf::RenderWindow& target);
    void drawGrid(sf::RenderTarget& target);
    void drawCameras(sf::RenderTarget& target);
    void drawSelection(sf::RenderTarget& target);
    void drawGizmo(sf::RenderTarget& target);
    void drawBrush(sf::RenderTarget& target);
    void updateCursor();

    sf::Vector2f toWorld(const QPointF& widgetPos);
    Entity pickEntity(sf::Vector2f world);
    float snapStep();

    // Gizmo de déplacement sur l'entité sélectionnée (outil Sélection) :
    // flèche rouge = X seulement, flèche verte = Y seulement, carré central = libre.
    enum class GizmoHandle { None, X, Y, Free };
    GizmoHandle gizmoHandleAt(sf::Vector2f world) const;
    // Peinture (Pinceau / Gomme / Remplir / Pipette)
    void beginStroke(sf::Vector2i cell);
    void strokeTo(sf::Vector2i cell);                 // de la dernière case peinte jusqu'à `cell`
    bool dab(sf::Vector2i cell);                      // un coup de pinceau ; true si la carte a changé
    void floodFill(sf::Vector2i cell);
    void pickAt(sf::Vector2i cell);
    void changeBrushSize(int delta);
    void updateHover(const QPointF& pos);
    QString describeCell(const TileMapData& map, TileCell cell) const;

    EditorContext& m_ctx;
    EditorCamera m_camera;
    LooseFileLoader m_textures{ "" };          // charge les PNG (id = chemin)
    std::unique_ptr<TilemapRenderSystem> m_tilemapRenderer;   // dessine les entités Tilemap
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
    sf::Vector2f m_dragStart;                       // position de l'entité au début du glisser
    GizmoHandle m_dragHandle = GizmoHandle::None;   // poignée du gizmo tenue (None : glisser libre sur le sprite)
    GizmoHandle m_hoverHandle = GizmoHandle::None;  // poignée sous la souris (surbrillance)

    sf::Vector2i m_hoveredTile{ 0, 0 };
    bool m_hoverValid = false;
    sf::Vector2i m_lastPainted{ INT32_MIN, INT32_MIN };   // dernière case du tracé en cours
    sf::Vector2i m_strokeOrigin;                          // tampon : la grille de pose part d'ici
    sf::Vector2i m_lastDab{ INT32_MIN, INT32_MIN };       // tampon : dernière case où il a été posé
    sf::Vector2i m_strokeEnd;                             // fin du dernier tracé (Maj + clic : ligne)
    bool m_hasStrokeEnd = false;
    bool m_strokeChanged = false;
};
