#pragma once
#include "ecs/entity.h"

#include <QSet>
#include <QString>
#include <QWidget>

class EditorContext;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Liste des entités de la scène, rangées dans des catégories (dossiers façon Unity).
//  - clic : sélectionner          - double-clic / F2 : renommer (entité ou catégorie)
//  - glisser une entité sur une catégorie : l'y ranger (ou dans le vide : la sortir)
//  - glisser une caméra sur une entité : la caméra la suit (Camera2d::targetLock) et s'affiche dessous
//  - Suppr : supprimer l'entité (ou la catégorie, ses entités sont gardées)
//  - bouton + ou clic droit : créer une entité / une catégorie
class HierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorContext& context, QWidget* parent = nullptr);

private:
    void rebuild();
    void syncSelection();
    void applyFilter();
    void showContextMenu(const QPoint& pos);
    void createEntityHere();
    void deleteCurrent();
    QString targetCategory(QTreeWidgetItem* item) const;   // catégorie d'un élément ("" = racine)
    void setFollowTarget(Entity camera, Entity target);     // Camera2d::targetLock (NULL_ENTITY = libre)
    QTreeWidgetItem* itemFor(Entity e) const;
    static bool isCategory(const QTreeWidgetItem* item);
    static Entity entityOf(const QTreeWidgetItem* item);

    EditorContext& m_ctx;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_tree = nullptr;
    QSet<QString> m_collapsed;   // catégories repliées
    bool m_updating = false;
};
