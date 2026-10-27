#pragma once
#include "ecs/entity.h"

#include <QWidget>

class EditorContext;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Liste des entités de la scène.
//  - clic : sélectionner   - double-clic / F2 : renommer   - Suppr : supprimer
//  - bouton + ou clic droit : créer une entité
class HierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorContext& context, QWidget* parent = nullptr);

private:
    void rebuild();
    void syncSelection();
    void applyFilter();
    void showContextMenu(const QPoint& pos);
    QTreeWidgetItem* itemFor(Entity e) const;
    static Entity entityOf(const QTreeWidgetItem* item);

    EditorContext& m_ctx;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_tree = nullptr;
    bool m_updating = false;
};
