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
//  - glisser une catégorie : sur une autre = dedans, entre deux = à côté, dans le vide = racine
//  - glisser une caméra sur une entité : la caméra la suit (CameraFollow) et s'affiche dessous
//  - Suppr : supprimer l'entité (ou la catégorie, ses entités sont gardées)
//  - bouton + ou clic droit : créer une entité / une catégorie
//  - sous une tilemap, ses layers (le plus haut est devant) : clic = layer actif, œil = cacher dans l'éditeur,
//    icône du perso = sous / trié / dessus, glisser = réordonner, + sur la tilemap = nouveau layer
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
    void setFollowTarget(Entity camera, Entity target);     // CameraFollow::target (NULL_ENTITY = libre)
    QTreeWidgetItem* itemFor(Entity e) const;
    QTreeWidgetItem* layerItem(Entity tilemap, int layer) const;
    void refreshLayerItems();      // layer actif et yeux, sans reconstruire la liste
    void moveLayers(QTreeWidgetItem* target, int pos);
    static bool isCategory(const QTreeWidgetItem* item);
    static bool isLayer(const QTreeWidgetItem* item);
    static bool isEntity(const QTreeWidgetItem* item);
    static Entity entityOf(const QTreeWidgetItem* item);
    static int layerOf(const QTreeWidgetItem* item);

    EditorContext& m_ctx;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_tree = nullptr;
    QSet<QString> m_collapsed;   // catégories repliées
    QSet<quint64> m_collapsedMaps;   // tilemaps repliées
    bool m_layerFocus = false;   // la ligne du layer actif est sélectionnée plutôt que la tilemap
    bool m_updating = false;
};
