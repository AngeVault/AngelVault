#include "editor_context.h"

#include "components/name.h"
#include "components/transform2d.h"

#include <QCoreApplication>
#include <QDir>

#include <algorithm>

EditorContext::EditorContext(QObject* parent) : QObject(parent) {}

std::vector<Entity> EditorContext::entities() const {
    std::vector<Entity> list;
    m_scene.forEachEntity([&](Entity e) { list.push_back(e); });
    return list;
}

QString EditorContext::displayName(Entity e) {
    if (m_scene.hasComponent<NameComponent>(e)) {
        const std::string& name = m_scene.getComponent<NameComponent>(e).name;
        if (!name.empty()) return QString::fromStdString(name);
    }
    return tr("Entité %1").arg(e.index);
}

Entity EditorContext::createEntity(const QString& name) {
    Entity e = m_scene.createEntity();
    m_scene.addComponent<NameComponent>(e, name.toStdString());
    m_scene.addComponent<Transform2D>(e);
    emit sceneChanged();
    select(e);
    return e;
}

void EditorContext::destroyEntity(Entity e) {
    if (!isValid(e)) return;
    const bool wasSelected = e == m_selected;
    m_scene.destroyEntity(e);
    m_scene.flushPendingDestroys();   // l'éditeur n'a pas de boucle de jeu : on applique tout de suite
    if (wasSelected) clearSelection();
    emit sceneChanged();
}

void EditorContext::renameEntity(Entity e, const QString& name) {
    if (!isValid(e)) return;
    if (m_scene.hasComponent<NameComponent>(e))
        m_scene.getComponent<NameComponent>(e).name = name.toStdString();
    else
        m_scene.addComponent<NameComponent>(e, name.toStdString());
    emit sceneChanged();
    emit entityModified(e, nullptr);
}

void EditorContext::select(Entity e) {
    if (!isValid(e)) e = NULL_ENTITY;
    if (e == m_selected) return;
    m_selected = e;
    emit selectionChanged(e);
}

void EditorContext::clearSelection() { select(NULL_ENTITY); }

void EditorContext::notifyEntityModified(Entity e, QObject* source) {
    if (isValid(e)) emit entityModified(e, source);
}

void EditorContext::setTool(EditorTool tool) {
    if (tool == m_tool) return;
    m_tool = tool;
    emit toolChanged(tool);
}

void EditorContext::setPaintType(TileType type) {
    if (type == m_paintType) return;
    m_paintType = type;
    emit paintTypeChanged(type);
}

void EditorContext::setBrushSize(int size) {
    size = std::clamp(size, 1, 9);
    if (size == m_brushSize) return;
    m_brushSize = size;
    emit brushSizeChanged(size);
}

QString tileTypeName(TileType t) {
    switch (t) {
    case TileType::Sand:        return QObject::tr("Sable");
    case TileType::Water:       return QObject::tr("Eau");
    case TileType::Grass:       return QObject::tr("Herbe");
    case TileType::BrickPath:   return QObject::tr("Pavés");
    case TileType::DirtPath:    return QObject::tr("Chemin de terre");
    case TileType::DonjonFloor: return QObject::tr("Sol de donjon");
    case TileType::Wall:        return QObject::tr("Mur");
    case TileType::Path:        return QObject::tr("Chemin");
    default:                    return QObject::tr("Vide");
    }
}

std::vector<TileType> paintableTileTypes() {
    return { TileType::Grass, TileType::Sand, TileType::Water,
             TileType::DirtPath, TileType::BrickPath, TileType::DonjonFloor };
}

QStringList tilesetFiles(TileType type) {
    const QString dir = QCoreApplication::applicationDirPath() + "/assets/tilesets/";
    auto file = [&dir](const char* name) { return QDir::toNativeSeparators(dir + name); };
    switch (type) {
    // herbe unie 3 fois sur 4, fleurs 1 fois sur 4 (répéter une feuille = plus fréquente)
    case TileType::Grass:       return { file("grass.png"), file("grass.png"), file("grass.png"), file("grass_flowers.png") };
    case TileType::Sand:        return { file("sand.png") };
    case TileType::Water:       return { file("water.png") };
    case TileType::DirtPath:    return { file("dirt_path.png") };
    case TileType::BrickPath:   return { file("stone_path.png") };
    case TileType::DonjonFloor: return { file("dungeon_floor.png") };
    default:                    return {};
    }
}
