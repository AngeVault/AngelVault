#include "editor_context.h"

#include "components/camera2d.h"
#include "components/name.h"
#include "components/persistent_id.h"
#include "components/script_component.h"
#include "ecs/behavior_factory.h"
#include "scene/prefab.h"
#include "components/transform2d.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <iostream>

namespace {
quint64 entityKey(Entity e) { return (quint64(e.index) << 32) | e.generation; }
}

EditorContext::EditorContext(QObject* parent) : QObject(parent) {
    // Toute modification de la scène la rend « modifiée » (sauf pendant un chargement)
    connect(this, &EditorContext::sceneChanged, this, [this] { if (!m_loading) setDirty(true); });
    connect(this, &EditorContext::entityModified, this, [this] { if (!m_loading) setDirty(true); });
}

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
    m_scene.addComponent<PersistentId>(e).value = generatePersistentId();   // identité pour la sauvegarde
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

// ---- Projet et scène sur le disque -------------------------------------------------------------

namespace {
std::string toUtf8(const QString& s) { return s.toUtf8().toStdString(); }
QString fromUtf8(const std::string& s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
}

QString EditorContext::projectDir() const {
    return hasProject() ? fromUtf8(m_project.directory()) : QString();
}

QString EditorContext::sceneName() const {
    return m_scenePath.isEmpty() ? tr("Sans titre") : QFileInfo(m_scenePath).fileName();
}

void EditorContext::setDirty(bool dirty) {
    if (m_dirty == dirty) return;
    m_dirty = dirty;
    emit dirtyChanged(dirty);
}

void EditorContext::resetScene() {
    m_selected = NULL_ENTITY;
    emit selectionChanged(NULL_ENTITY);
    clearRegistry(m_scene);
    m_categories.clear();
    m_entityCategory.clear();
}

Json EditorContext::editorData() {
    Json categories = Json::array();
    for (const QString& c : m_categories) categories.push_back(toUtf8(c));
    Json entityCategory = Json::object();
    for (Entity e : entities()) {
        const QString category = categoryOf(e);
        if (category.isEmpty() || !m_scene.hasComponent<PersistentId>(e)) continue;
        entityCategory[std::to_string(m_scene.getComponent<PersistentId>(e).value)] = toUtf8(category);
    }
    return Json{ { "categories", categories }, { "entityCategory", entityCategory } };
}

void EditorContext::applyEditorData(const Json& editor) {
    m_categories.clear();
    m_entityCategory.clear();
    if (editor.contains("categories") && editor["categories"].is_array())
        for (const Json& c : editor["categories"])
            if (c.is_string() && !m_categories.contains(fromUtf8(c.get<std::string>()))) m_categories.push_back(fromUtf8(c.get<std::string>()));
    if (!editor.contains("entityCategory") || !editor["entityCategory"].is_object()) return;
    const Json& map = editor["entityCategory"];
    for (Entity e : entities()) {
        if (!m_scene.hasComponent<PersistentId>(e)) continue;
        const auto it = map.find(std::to_string(m_scene.getComponent<PersistentId>(e).value));
        if (it == map.end() || !it->is_string()) continue;
        const QString category = fromUtf8(it->get<std::string>());
        if (m_categories.contains(category)) m_entityCategory.insert(entityKey(e), category);
    }
}

SceneMeta EditorContext::sceneMeta() {
    SceneMeta meta;
    meta.name = toUtf8(QFileInfo(m_scenePath).completeBaseName());
    meta.editor = editorData();
    return meta;
}

bool EditorContext::openProject(const QString& path, QString* error) {
    Project project;
    std::string err;
    if (!project.load(toUtf8(path), &err)) { if (error) *error = fromUtf8(err); return false; }
    m_project = std::move(project);
    const QString start = fromUtf8(m_project.resolve(m_project.startScene));
    if (QFileInfo::exists(start)) {
        if (!openScene(start, error)) newScene();   // fichier abîmé : on ne l'écrase pas (Enregistrer sous)
    } else {
        newScene();
        m_scenePath = start;   // pas encore de fichier : le premier Ctrl+S le crée
    }
    emit documentChanged();
    return true;
}

bool EditorContext::openScene(const QString& path, QString* error) {
    std::string text, err;
    if (!readFile(toUtf8(path), text, &err)) { if (error) *error = fromUtf8(err); return false; }
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", std::string()) != "angeVault.scene") {
        if (error) *error = tr("%1 n'est pas une scène AngeVault valide").arg(QDir::toNativeSeparators(path));
        return false;
    }

    m_loading = true;
    resetScene();
    SceneMeta meta;
    const SceneLoadResult result = sceneFromJson(m_scene, meta, j);
    for (const std::string& w : result.warnings) std::cerr << "[Scene] " << w << std::endl;
    applyEditorData(meta.editor);
    snapAllFollowers();
    m_scenePath = QFileInfo(path).absoluteFilePath();
    emit sceneChanged();
    m_loading = false;
    setDirty(false);
    emit documentChanged();
    std::cout << "[Scene] " << toUtf8(QDir::toNativeSeparators(m_scenePath)) << " : "
              << result.entities.size() << " entites" << std::endl;
    if (!result.ok && error) *error = fromUtf8(result.error);
    return result.ok;
}

bool EditorContext::saveScene(QString* error) {
    if (m_scenePath.isEmpty()) { if (error) *error = tr("La scène n'a pas encore de fichier."); return false; }
    std::string err;
    if (!::saveScene(m_scene, sceneMeta(), toUtf8(m_scenePath), &err)) { if (error) *error = fromUtf8(err); return false; }
    if (hasProject()) {   // scène connue du projet (liste de l'éditeur, scène de départ si c'est la première)
        const std::string rel = m_project.relative(toUtf8(m_scenePath));
        const std::size_t before = m_project.scenes.size();
        m_project.addScene(rel);
        if (m_project.scenes.size() != before) m_project.save(&err);
    }
    setDirty(false);
    std::cout << "[Scene] Enregistree : " << toUtf8(QDir::toNativeSeparators(m_scenePath)) << std::endl;
    return true;
}

bool EditorContext::saveSceneAs(const QString& path, QString* error) {
    const QString previous = m_scenePath;
    m_scenePath = QFileInfo(path).absoluteFilePath();
    if (!saveScene(error)) { m_scenePath = previous; return false; }
    emit documentChanged();
    return true;
}

void EditorContext::newScene() {
    m_loading = true;
    resetScene();
    m_scenePath.clear();
    emit sceneChanged();
    m_loading = false;
    setDirty(false);
    emit documentChanged();
}

std::string EditorContext::sceneSnapshot() {
    return sceneToString(m_scene, sceneMeta());
}

void EditorContext::importEntities(const std::function<void(Registry&)>& fill) {
    fill(m_scene);
    ensurePersistentIds(m_scene);
    snapAllFollowers();
    emit sceneChanged();
}

// ---- Comportements et prefabs --------------------------------------------------------------------

bool EditorContext::addBehavior(Entity e, const std::string& name) {
    if (!isValid(e)) return false;
    std::unique_ptr<IBehavior> b = BehaviorFactory::get().create(name);
    if (!b) return false;
    Script& script = m_scene.hasComponent<Script>(e) ? m_scene.getComponent<Script>(e) : m_scene.addComponent<Script>(e);
    script.add(std::move(b));
    emit entityModified(e, nullptr);
    emit componentsChanged(e);
    return true;
}

void EditorContext::removeBehavior(Entity e, std::size_t index) {
    if (!isValid(e) || !m_scene.hasComponent<Script>(e)) return;
    Script& script = m_scene.getComponent<Script>(e);
    script.remove(index);
    if (script.empty()) m_scene.removeComponent<Script>(e);
    emit entityModified(e, nullptr);
    emit componentsChanged(e);
}

QByteArray EditorContext::entitiesToMime(const std::vector<Entity>& entities) {
    QByteArray data;
    for (Entity e : entities) data += QByteArray::number(e.index) + ':' + QByteArray::number(e.generation) + '\n';
    return data;
}

std::vector<Entity> EditorContext::entitiesFromMime(const QByteArray& data) const {
    std::vector<Entity> out;
    for (const QByteArray& line : data.split('\n')) {
        const QList<QByteArray> parts = line.split(':');
        if (parts.size() != 2) continue;
        const Entity e{ parts[0].toUInt(), parts[1].toUInt() };
        if (isValid(e)) out.push_back(e);
    }
    return out;
}

QString EditorContext::defaultPrefabPath(Entity root, const QString& folder) {
    QString base = displayName(root).trimmed();
    for (QChar& ch : base) if (QString("\\/:*?\"<>|").contains(ch)) ch = '_';   // caractères interdits (Windows)
    if (base.isEmpty()) base = "Prefab";
    const QString dir = !folder.isEmpty() ? folder : (hasProject() ? projectDir() : QDir::homePath()) + "/prefabs";
    QString path = dir + "/" + base + kPrefabExtension;
    for (int i = 2; QFileInfo::exists(path); ++i) path = QString("%1/%2 %3%4").arg(dir, base).arg(i).arg(kPrefabExtension);
    return path;
}

bool EditorContext::createPrefab(Entity root, const QString& path, QString* error) {
    if (!isValid(root)) { if (error) *error = tr("Aucune entité."); return false; }
    std::string err;
    const std::vector<Entity> group = prefabGroup(m_scene, root);
    const std::string name = toUtf8(QFileInfo(path).completeBaseName());
    if (!savePrefab(m_scene, group, name, toUtf8(path), &err)) { if (error) *error = fromUtf8(err); return false; }
    emit statusMessage(tr("Prefab créé : %1 (%2 entité(s))").arg(QDir::toNativeSeparators(path)).arg(group.size()));
    std::cout << "[Prefab] Cree : " << toUtf8(QDir::toNativeSeparators(path)) << std::endl;
    return true;
}

std::vector<Entity> EditorContext::instantiatePrefab(const QString& path, std::optional<sf::Vector2f> position, QString* error) {
    std::string text, err;
    if (!readFile(toUtf8(path), text, &err)) { if (error) *error = fromUtf8(err); return {}; }
    const Json j = Json::parse(text, nullptr, false);   // relu à chaque fois : le prefab a pu changer
    const SceneLoadResult r = ::instantiatePrefab(m_scene, j, position);
    for (const std::string& w : r.warnings) std::cerr << "[Prefab] " << w << std::endl;
    if (!r.ok) { if (error) *error = tr("%1 : %2").arg(QFileInfo(path).fileName(), fromUtf8(r.error)); return {}; }
    snapAllFollowers();
    emit sceneChanged();
    if (!r.entities.empty()) select(r.entities.front());
    return r.entities;
}

std::vector<Entity> EditorContext::dropFiles(const QStringList& paths, Entity target, std::optional<sf::Vector2f> position) {
    std::vector<Entity> created;
    QStringList done, problems;
    for (const QString& path : paths) {
        const QFileInfo fi(path);
        const QString ext = fi.suffix().toLower();
        if ("." + ext == QString(kPrefabExtension)) {
            QString err;
            const std::vector<Entity> made = instantiatePrefab(path, position, &err);
            if (made.empty()) problems << err;
            else { created.insert(created.end(), made.begin(), made.end()); done << fi.completeBaseName(); }
        } else if (ext == "h" || ext == "hpp") {
            const std::vector<std::string> names = BehaviorFactory::get().namesInFile(toUtf8(fi.fileName()));
            if (names.empty()) {
                problems << tr("« %1 » : aucun comportement compilé (REGISTER_BEHAVIOR manquant, ou recompilez l'éditeur)").arg(fi.fileName());
            } else if (!isValid(target)) {
                problems << tr("Déposez « %1 » sur une entité").arg(fi.fileName());
            } else {
                for (const std::string& n : names) if (addBehavior(target, n)) done << fromUtf8(n);
            }
        } else {
            problems << tr("« %1 » : ni un prefab, ni un comportement (.h)").arg(fi.fileName());
        }
    }
    if (!problems.isEmpty()) emit statusMessage(problems.join("  —  "));
    else if (!done.isEmpty()) emit statusMessage(tr("Ajouté : %1").arg(done.join(", ")));
    return created;
}
