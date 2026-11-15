#include "scene/scene_transition.h"
#include "scene/scene_io.h"
#include "scene/tilemaps.h"

#include "components/dont_destroy.h"
#include "components/rigid_body2d.h"
#include "components/spawn_point.h"
#include "components/transform2d.h"
#include "components/camera_follow.h"

#include <filesystem>

namespace {
namespace fs = std::filesystem;

// chemin relatif au projet -> chemin complet
std::string join(const std::string& dir, const std::string& rel) {
    const fs::path p(std::u8string(rel.begin(), rel.end()));
    if (p.is_absolute() || dir.empty()) return rel;
    const fs::path full = fs::path(std::u8string(dir.begin(), dir.end())) / p;
    const std::u8string s = full.lexically_normal().generic_u8string();
    return std::string(s.begin(), s.end());
}
} // namespace

SceneChangeResult changeScene(Registry& registry, const std::string& projectDir,
                              const std::string& scenePath, const std::string& spawnName) {
    SceneChangeResult result;

    clearTransientEntities(registry);

    SceneMeta meta;
    const SceneLoadResult scene = loadScene(registry, meta, join(projectDir, scenePath));
    result.warnings = scene.warnings;
    if (!scene.ok) { result.error = scene.error; return result; }

    loadTilemaps(registry, projectDir, &result.warnings);

    // place le joueur sur le point d'arrivée
    Entity spawn = NULL_ENTITY;
    registry.view<SpawnPoint>([&](Entity e, SpawnPoint& sp) { if (sp.name == spawnName) spawn = e; });
    if (spawn == NULL_ENTITY) {
        if (!spawnName.empty()) result.warnings.push_back("SpawnPoint introuvable : " + spawnName);
    } else if (registry.hasComponent<Transform2D>(spawn)) {
        const sf::Vector2f pos = registry.getComponent<Transform2D>(spawn).getPosition();
        registry.view<DontDestroy, Transform2D>([&](Entity e, DontDestroy& d, Transform2D& t) {
            if (d.key != "Player") return;
            t.setPosition(pos);
            if (registry.hasComponent<RigidBody2D>(e)) registry.getComponent<RigidBody2D>(e).velocity = {};
        });
    }

    // la caméra se recale d'un coup
    registry.view<CameraFollow>([&](Entity, CameraFollow& f) { f.started = false; });

    result.ok = true;
    return result;
}
