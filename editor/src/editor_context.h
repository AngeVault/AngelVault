#pragma once
#include "RtsTile_map.hpp"
#include "ecs/registry.h"

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
    TileMap& map() { return m_map; }

    // --- Entités ---
    std::vector<Entity> entities() const;
    bool isValid(Entity e) const { return m_scene.isValid(e); }
    QString displayName(Entity e);
    Entity createEntity(const QString& name = tr("Nouvelle entité"));
    void destroyEntity(Entity e);
    void renameEntity(Entity e, const QString& name);

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
    void toolChanged(EditorTool tool);
    void paintTypeChanged(TileType type);
    void brushSizeChanged(int size);

private:
    Registry m_scene;
    TileMap m_map;
    Entity m_selected = NULL_ENTITY;

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
