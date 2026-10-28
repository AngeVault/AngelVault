#pragma once
#include "RtsTile_map.hpp"
#include "ecs/registry.h"
#include "RtsTile_map.hpp"
#include "Layer.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>


// Outils de la vue scène (choisis dans la Tile Palette)
enum class EditorTool { Select, Paint, Erase, Pick };

// État partagé de l'éditeur : la scène ouverte, la tilemap, la sélection et l'outil actif.
// Tous les panneaux (Hierarchy, Inspector, vue scène...) lisent et modifient CET objet
// et se tiennent à jour grâce à ses signaux : aucun panneau ne parle directement à un autre.
class EditorContext : public QObject {
    Q_OBJECT
public:
    explicit EditorContext(QObject* parent = nullptr);

    Registry& scene() { return m_scene; }
    // --- Map and layers ---
    TileMap& map() { return m_map[m_layer.getLayerIndex(m_selectedLayer->getName())]; }
    void createLayer(const std::string& name, LayersType type);

    // --- Entités ---
    std::vector<Entity> entities() const;
    bool isValid(Entity e) const { return m_scene.isValid(e); }
    QString displayName(Entity e);
    Entity createEntity(const QString& name = tr("Nouvelle entité"));
    void destroyEntity(Entity e);
    void renameEntity(Entity e, const QString& name);

    // --- Catégories (Hierarchy) ---
    // Dossiers de l'éditeur pour ranger les entités, comme les objets vides "dossier" d'Unity.
    // Ils n'existent que dans l'éditeur : rien n'est ajouté aux entités du jeu.
    QStringList categories() const { return m_categories; }
    QString addCategory(const QString& name = tr("Nouvelle catégorie"));   // renvoie le nom (rendu unique)
    void renameCategory(const QString& oldName, const QString& newName);
    void removeCategory(const QString& name);                              // ses entités reviennent à la racine
    QString categoryOf(Entity e) const;                                    // "" = aucune catégorie
    void setCategory(Entity e, const QString& category);                   // "" = racine

    // --- Caméras liées (Camera2d::targetLock) ---
    // Une caméra qui suit une entité est placée sur elle et ne peut plus être déplacée à la main :
    // sa position est dictée par la cible (c'est ce que fait le CameraSystem en jeu).
    Entity followTarget(Entity camera);                       // NULL_ENTITY si libre (ou pas une caméra)
    bool isPositionLocked(Entity e) { return isValid(followTarget(e)); }
    void setFollowTarget(Entity camera, Entity target);       // NULL_ENTITY = libérer la caméra
    void snapFollowers(Entity target, int depth = 0);         // replace les caméras qui suivent `target`
    void snapAllFollowers();

    // --- Sélection ---
    bool hasSelection() const { return isValid(m_selected); }
    Entity selected() const { return m_selected; }
    void select(Entity e);
    void clearSelection();

    // À appeler après avoir modifié une entité ; `source` = l'émetteur (pour ne pas se rafraîchir soi-même)
    void notifyEntityModified(Entity e, QObject* source = nullptr);
    void notifySceneChanged() { emit sceneChanged(); }

    // --- Outils de la Tile Palette ---
    EditorTool tool() const { return m_tool; }
    void setTool(EditorTool tool);
    TileType paintType() const { return m_paintType; }
    void setPaintType(TileType type);
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int size);

signals:
    void sceneChanged();                          // entités ajoutées / supprimées / renommées
    void selectionChanged(Entity e);              // NULL_ENTITY si plus rien n'est sélectionné
    void entityModified(Entity e, QObject* source);
    void followTargetChanged(Entity camera);      // la caméra suit une autre entité (ou plus rien)
    void toolChanged(EditorTool tool);
    void paintTypeChanged(TileType type);
    void brushSizeChanged(int size);

private:
    Registry m_scene;
    LayerManager m_layer;
    Layer* m_selectedLayer;
    std::vector<TileMap> m_map;
    Entity m_selected = NULL_ENTITY;
    QStringList m_categories;
    QHash<quint64, QString> m_entityCategory;   // clé : index + génération de l'entité

    EditorTool m_tool = EditorTool::Select;
    TileType m_paintType = TileType::Grass;
    int m_brushSize = 1;
};

// Nom lisible d'un type de tile (palette, barre d'état)
QString tileTypeName(TileType type);

// Terrains peignables, dans l'ordre de la palette
std::vector<TileType> paintableTileTypes();

// Feuilles de tiles d'un terrain (chemins absolus, dans bin/assets/tilesets).
// Plusieurs feuilles = variantes ; la première sert aussi aux transitions.
QStringList tilesetFiles(TileType type);
