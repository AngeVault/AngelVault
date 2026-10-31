#pragma once
#include "editor_context.h"

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;
class QListWidget;
class QSpinBox;

// Outils de la vue scène + tiles de la tilemap active (Hierarchy : entité avec un composant Tilemap).
//  V : sélection   B : pinceau   E : gomme   I : pipette
// Les tiles ne sont pas codées en dur : + Tile pour en ajouter (simple ou autotile).
// L'ordre de la liste = l'empilement (une tile plus bas déborde sur celles du dessus).
class TilePalettePanel : public QWidget {
    Q_OBJECT
public:
    explicit TilePalettePanel(EditorContext& context, QWidget* parent = nullptr);

    // Raccourcis V / B / E / I : à ajouter à la fenêtre principale (window.addActions)
    // pour qu'ils marchent même quand ce panneau est caché ou dans un onglet de fond.
    QList<QAction*> toolActions() const { return m_toolActions; }

private:
    void buildTerrains();
    void syncTool(EditorTool tool);
    void syncTerrain(uint16_t id);
    void editTile(int index);      // -1 = nouvelle
    void removeTile();
    void moveTile(int delta);      // ordre d'empilement

    EditorContext& m_ctx;
    QButtonGroup* m_tools = nullptr;
    QListWidget* m_terrains = nullptr;
    class QLabel* m_mapInfo = nullptr;
    class QWidget* m_tileButtons = nullptr;
    QSpinBox* m_brush = nullptr;
    QList<QAction*> m_toolActions;
};
