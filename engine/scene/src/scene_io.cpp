#include "scene/scene_io.h"
#include "scene/project.h"

#include "components/persistent_id.h"

#include <unordered_map>
#include <unordered_set>

namespace {
std::vector<Entity> aliveEntities(Registry& registry) {
    std::vector<Entity> out;
    registry.forEachEntity([&](Entity e) { out.push_back(e); });
    return out;
}
} // namespace

void ensurePersistentIds(Registry& registry) {
    std::unordered_set<uint64_t> used;
    for (Entity e : aliveEntities(registry)) {
        if (!registry.hasComponent<PersistentId>(e)) continue;
        PersistentId& id = registry.getComponent<PersistentId>(e);
        if (id.value == 0 || !used.insert(id.value).second) id.value = 0;   // doublon (copie) : nouvel id plus bas
    }
    for (Entity e : aliveEntities(registry)) {
        PersistentId& id = registry.hasComponent<PersistentId>(e) ? registry.getComponent<PersistentId>(e)
                                                                  : registry.addComponent<PersistentId>(e);
        if (id.value != 0) continue;   // déjà un identifiant valide : on n'y touche pas
        do id.value = generatePersistentId(); while (used.count(id.value));
        used.insert(id.value);
    }
}

void clearRegistry(Registry& registry) {
    for (Entity e : aliveEntities(registry)) registry.destroyEntity(e);
    registry.flushPendingDestroys();
}

// ---- Écriture --------------------------------------------------------------------------------

Json entitiesToJson(Registry& registry, const std::vector<Entity>& list) {
    ensurePersistentIds(registry);
    // Liens vers une entité HORS de la liste : écrits « null » (ex. prefab d'une caméra sans sa cible)
    std::unordered_set<uint64_t> included;
    for (Entity e : list)
        if (registry.isValid(e)) included.insert(registry.getComponent<PersistentId>(e).value);

    SaveContext ctx;
    ctx.idOf = [&registry, &included](Entity e) -> uint64_t {
        if (!registry.isValid(e) || !registry.hasComponent<PersistentId>(e)) return 0;
        const uint64_t id = registry.getComponent<PersistentId>(e).value;
        return included.count(id) ? id : 0;
    };

    Json entities = Json::array();
    for (Entity e : list) {
        if (!registry.isValid(e)) continue;
        Json components = Json::object();
        for (const ComponentSerializer& s : SerializerRegistry::get().all())
            if (s.has(registry, e)) components[s.name] = s.save(registry, e, ctx);
        entities.push_back({ { "id", registry.getComponent<PersistentId>(e).value }, { "components", components } });
    }
    return entities;
}

Json sceneToJson(Registry& registry, const SceneMeta& meta) {
    const Json entities = entitiesToJson(registry, aliveEntities(registry));

    Json j;
    j["format"] = "angeVault.scene";
    j["version"] = kSceneFormatVersion;
    j["name"] = meta.name;
    j["map"] = meta.mapPath.empty() ? Json(nullptr) : Json(meta.mapPath);
    j["entities"] = entities;
    j["editor"] = meta.editor.is_object() ? meta.editor : Json::object();
    return j;
}

std::string sceneToString(Registry& registry, const SceneMeta& meta) {
    return sceneToJson(registry, meta).dump(2);
}

bool saveScene(Registry& registry, const SceneMeta& meta, const std::string& path, std::string* error) {
    return writeFileAtomic(path, sceneToString(registry, meta) + "\n", error);
}

// ---- Lecture ---------------------------------------------------------------------------------

SceneLoadResult entitiesFromJson(Registry& registry, const Json& entities, bool freshIds) {
    SceneLoadResult result;
    if (!entities.is_array()) { result.error = "\"entities\" doit être une liste"; return result; }

    // Passe 1 : créer toutes les entités, et la table « id du fichier -> Entity »
    std::unordered_map<uint64_t, Entity> table;
    std::unordered_set<uint64_t> usedIds;
    if (freshIds)   // copie (prefab) : les nouveaux ids ne doivent pas exister déjà dans le registry
        registry.forEachEntity([&](Entity e) {
            if (registry.hasComponent<PersistentId>(e)) usedIds.insert(registry.getComponent<PersistentId>(e).value);
        });
    for (const Json& je : entities) {
        Entity e = registry.createEntity();
        const uint64_t fileId = SceneJson::value<uint64_t>(je, "id", 0);
        uint64_t id = fileId;
        if (freshIds || id == 0 || usedIds.count(id)) {   // copie, absent ou en double : nouvel id
            if (!freshIds && id != 0) result.warnings.push_back("identifiant d'entité en double : " + std::to_string(id));
            do id = generatePersistentId(); while (usedIds.count(id));
        }
        usedIds.insert(id);
        registry.addComponent<PersistentId>(e).value = id;
        if (fileId != 0 && !table.count(fileId)) table[fileId] = e;
        result.entities.push_back(e);
    }

    // Passe 2 : les composants, en traduisant les identifiants du fichier
    LoadContext ctx;
    ctx.entityOf = [&table](uint64_t id) -> Entity {
        const auto it = table.find(id);
        return it != table.end() ? it->second : NULL_ENTITY;
    };
    ctx.warn = [&result](const std::string& w) { result.warnings.push_back(w); };

    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity e = result.entities[i];
        const Json components = entities[i].value("components", Json::object());
        if (!components.is_object()) continue;
        for (auto it = components.begin(); it != components.end(); ++it) {
            const ComponentSerializer* s = SerializerRegistry::get().find(it.key());
            if (!s) { result.warnings.push_back("composant inconnu ignoré : " + it.key()); continue; }
            try {
                s->load(registry, e, it.value(), ctx);
            } catch (const std::exception& ex) {
                result.warnings.push_back(it.key() + " : " + ex.what());
            }
        }
    }
    result.ok = true;
    return result;
}

SceneLoadResult sceneFromJson(Registry& registry, SceneMeta& meta, const Json& j) {
    SceneLoadResult result;
    if (!j.is_object() || j.value("format", std::string()) != "angeVault.scene") {
        result.error = "ce n'est pas un fichier de scène AngeVault (champ \"format\")";
        return result;
    }
    const int version = SceneJson::value(j, "version", 1);
    if (version > kSceneFormatVersion) {
        result.error = "scène créée par une version plus récente du moteur (version " + std::to_string(version) + ")";
        return result;
    }
    const Json entities = j.value("entities", Json::array());
    if (!entities.is_array()) { result.error = "\"entities\" doit être une liste"; return result; }

    meta.name = SceneJson::value<std::string>(j, "name", "");
    meta.mapPath = SceneJson::value<std::string>(j, "map", "");
    meta.editor = j.value("editor", Json::object());
    if (!meta.editor.is_object()) meta.editor = Json::object();

    SceneLoadResult loaded = entitiesFromJson(registry, entities, false);
    loaded.warnings.insert(loaded.warnings.begin(), result.warnings.begin(), result.warnings.end());
    return loaded;
}

SceneLoadResult sceneFromString(Registry& registry, SceneMeta& meta, const std::string& text) {
    Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded()) {
        SceneLoadResult r;
        r.error = "JSON invalide";
        return r;
    }
    return sceneFromJson(registry, meta, j);
}

SceneLoadResult loadScene(Registry& registry, SceneMeta& meta, const std::string& path) {
    std::string text, error;
    if (!readFile(path, text, &error)) {
        SceneLoadResult r;
        r.error = error;
        return r;
    }
    SceneLoadResult r = sceneFromString(registry, meta, text);
    if (!r.ok) r.error = path + " : " + r.error;
    return r;
}
