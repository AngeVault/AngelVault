#include "editor_context.h"

#include "components/camera2d.h"
#include "components/name.h"
#include "components/transform2d.h"

#include <QCoreApplication>
#include <QDir>

#include <algorithm>

namespace {
quint64 entityKey(Entity e) { return (quint64(e.index) << 32) | e.generation; }
}

EditorContext::EditorContext(QObject* parent) : QObject(parent) {}

QString EditorContext::addCategory(const QString& name) {
    QString base = name.trimmed().isEmpty() ? tr("Nouvelle catégorie") : name.trimmed();
    QString unique = base;
    for (int i = 2; m_categories.contains(unique); ++i) unique = QString("%1 %2").arg(base).arg(i);
    m_categories.push_back(unique);
    emit sceneChanged();
    return unique;
}

void EditorContext::renameCategory(const QString& oldName, const QString& newName) {
    const QString name = newName.trimmed();
    const int i = m_categories.indexOf(oldName);
    if (i < 0 || name.isEmpty() || name == oldName || m_categories.contains(name)) { emit sceneChanged(); return; }
    m_categories[i] = name;
    for (auto it = m_entityCategory.begin(); it != m_entityCategory.end(); ++it)
        if (it.value() == oldName) it.value() = name;
    emit sceneChanged();
}

void EditorContext::removeCategory(const QString& name) {
    if (!m_categories.removeOne(name)) return;
    for (auto it = m_entityCategory.begin(); it != m_entityCategory.end();)
        it = (it.value() == name) ? m_entityCategory.erase(it) : std::next(it);
    emit sceneChanged();
}

QString EditorContext::categoryOf(Entity e) const {
    return m_entityCategory.value(entityKey(e));
}

void EditorContext::setCategory(Entity e, const QString& category) {
    if (!isValid(e)) return;
    if (category.isEmpty() || !m_categories.contains(category)) m_entityCategory.remove(entityKey(e));
    else m_entityCategory.insert(entityKey(e), category);
    emit sceneChanged();
}

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
    m_entityCategory.remove(entityKey(e));
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
    if (!isValid(e)) return;
    emit entityModified(e, source);
    snapFollowers(e);   // la cible a peut-être bougé : ses caméras suivent
}

Entity EditorContext::followTarget(Entity camera) {
    if (!isValid(camera) || !m_scene.hasComponent<Camera2d>(camera)) return NULL_ENTITY;
    const Entity target = m_scene.getComponent<Camera2d>(camera).targetLock;
    return (target != camera && isValid(target)) ? target : NULL_ENTITY;
}

void EditorContext::setFollowTarget(Entity camera, Entity target) {
    if (!isValid(camera) || !m_scene.hasComponent<Camera2d>(camera)) return;
    if (m_scene.getComponent<Camera2d>(camera).targetLock == target) return;
    m_scene.getComponent<Camera2d>(camera).targetLock = target;   // même donnée que Camera2d::setTarget()
    if (isValid(target) && m_scene.hasComponent<Transform2D>(target) && m_scene.hasComponent<Transform2D>(camera))
        m_scene.getComponent<Transform2D>(camera).setPosition(m_scene.getComponent<Transform2D>(target).getPosition());
    emit followTargetChanged(camera);
    emit entityModified(camera, nullptr);
    snapFollowers(camera);
    emit sceneChanged();   // la Hierarchy se réorganise
}

void EditorContext::snapFollowers(Entity target, int depth) {
    if (depth > 8 || !m_scene.hasComponent<Transform2D>(target)) return;   // garde-fou : chaîne/boucle de caméras
    const sf::Vector2f pos = m_scene.getComponent<Transform2D>(target).getPosition();
    std::vector<Entity> moved;
    m_scene.view<Camera2d, Transform2D>([&](Entity cam, Camera2d& c, Transform2D& t) {
        if (cam == target || c.targetLock != target || t.getPosition() == pos) return;
        t.setPosition(pos);
        moved.push_back(cam);
    });
    for (Entity cam : moved) {
        emit entityModified(cam, nullptr);
        snapFollowers(cam, depth + 1);
    }
}

void EditorContext::snapAllFollowers() {
    for (Entity e : entities()) snapFollowers(e);
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
