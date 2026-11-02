#include "scene/prefab.h"
#include "scene/project.h"

#include "components/camera2d.h"
#include "components/transform2d.h"

#include <algorithm>
#include <unordered_map>

std::vector<Entity> prefabGroup(Registry& registry, Entity root) {
    std::vector<Entity> group;
    if (!registry.isValid(root)) return group;
    group.push_back(root);
    // Caméras qui suivent une entité du groupe (et ce qui les suit), sans boucle
    for (std::size_t i = 0; i < group.size(); ++i) {
        const Entity target = group[i];
        registry.view<Camera2d>([&](Entity cam, Camera2d& c) {
            if (c.targetLock == target && std::find(group.begin(), group.end(), cam) == group.end())
                group.push_back(cam);
        });
    }
    return group;
}

Json prefabToJson(Registry& registry, const std::vector<Entity>& entities, const std::string& name) {
    Json j;
    j["format"] = "angeVault.prefab";
    j["version"] = kPrefabFormatVersion;
    j["name"] = name;
    j["entities"] = entitiesToJson(registry, entities);
    return j;
}

bool savePrefab(Registry& registry, const std::vector<Entity>& entities, const std::string& name,
                const std::string& path, std::string* error) {
    if (entities.empty()) { if (error) *error = "prefab vide"; return false; }
    return writeFileAtomic(path, prefabToJson(registry, entities, name).dump(2) + "\n", error);
}

std::string prefabName(const Json& prefab) {
    return prefab.is_object() ? SceneJson::value<std::string>(prefab, "name", "") : std::string();
}

SceneLoadResult instantiatePrefab(Registry& registry, const Json& prefab, std::optional<sf::Vector2f> position) {
    SceneLoadResult result;
    if (!prefab.is_object() || prefab.value("format", std::string()) != "angeVault.prefab") {
        result.error = "ce n'est pas un prefab AngeVault (champ \"format\")";
        return result;
    }
    if (SceneJson::value(prefab, "version", 1) > kPrefabFormatVersion) {
        result.error = "prefab créé par une version plus récente du moteur";
        return result;
    }
    result = entitiesFromJson(registry, prefab.value("entities", Json::array()), true);   // nouveaux ids
    if (!result.ok || !position || result.entities.empty()) return result;

    // Déplace tout le groupe pour que la racine soit à `position`
    const Entity root = result.entities.front();
    if (!registry.hasComponent<Transform2D>(root)) return result;
    const sf::Vector2f delta = *position - registry.getComponent<Transform2D>(root).getPosition();
    for (Entity e : result.entities)
        if (registry.hasComponent<Transform2D>(e)) registry.getComponent<Transform2D>(e).move(delta);
    return result;
}

SceneLoadResult instantiatePrefabFile(Registry& registry, const std::string& path, std::optional<sf::Vector2f> position) {
    // Petit cache : un prefab instancié souvent (projectiles...) n'est lu sur le disque qu'une fois
    static std::unordered_map<std::string, Json> cache;
    auto it = cache.find(path);
    if (it == cache.end()) {
        std::string text, error;
        if (!readFile(path, text, &error)) { SceneLoadResult r; r.error = error; return r; }
        Json j = Json::parse(text, nullptr, false);
        if (j.is_discarded()) { SceneLoadResult r; r.error = path + " : JSON invalide"; return r; }
        it = cache.emplace(path, std::move(j)).first;
    }
    return instantiatePrefab(registry, it->second, position);
}
