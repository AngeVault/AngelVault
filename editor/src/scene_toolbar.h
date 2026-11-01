#pragma once
#include "editor_context.h"

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;

// Barre d'outils au-dessus de la vue scène : outils qui agissent sur les entités (pas sur la tilemap).
//  V : sélection / déplacement     R : taille (forme et / ou collision de l'entité sélectionnée)
// Avec l'outil Taille, on choisit ce que l'on agrandit : forme + collision, forme seule ou collision seule.
// Les outils de la tilemap (pinceau, gomme...) restent dans la Tile Palette.
class SceneToolbar : public QWidget {
    Q_OBJECT
public:
    explicit SceneToolbar(EditorContext& context, QWidget* parent = nullptr);

    // Raccourci R (V est celui de la Tile Palette) : à ajouter à la fenêtre principale (window.addActions)
    QList<QAction*> toolActions() const { return m_toolActions; }

private:
    void syncTool(EditorTool tool);

    EditorContext& m_ctx;
    QButtonGroup* m_tools = nullptr;
    QWidget* m_resizeBox = nullptr;
    QList<QAction*> m_toolActions;
};
