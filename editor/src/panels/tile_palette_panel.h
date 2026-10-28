#pragma once
#include "editor_context.h"

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;
class QListWidget;
class QSpinBox;

// Outils de la vue scène + terrains à peindre.
//  V : sélection   B : pinceau   E : gomme   I : pipette
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
    void syncTerrain(TileType type);

    EditorContext& m_ctx;
    QButtonGroup* m_tools = nullptr;
    QListWidget* m_terrains = nullptr;
    QSpinBox* m_brush = nullptr;
    QList<QAction*> m_toolActions;
};
