#include "scene/scene_transition.h"
#include "scene/scene_io.h"

#include "components/camera_follow.h"
#include "components/dont_destroy.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include "components/spawn_point.h"
#include "components/transform2d.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

static Entity findByKey(Registry& r, const std::string& key) {
    Entity found = NULL_ENTITY;
    r.view<DontDestroy>([&](Entity e, DontDestroy& d) { if (d.key == key) found = e; });
    return found;
}

static int aliveCount(Registry& r) {
    int n = 0;
    r.forEachEntity([&](Entity) { ++n; });
    return n;
}

static void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary);
    f << text;
}

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "angevault_test_transition";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    const std::string mainJson = R"({
        "format": "angeVault.scene", "version": 1, "name": "main",
        "entities": [
            { "id": 1, "components": { "Name": { "name": "Player" }, "Transform2D": { "position": [0, 0] },
                                        "RigidBody2D": {}, "DontDestroy": { "key": "Player" } } },
            { "id": 2, "components": { "Name": { "name": "Camera" }, "Transform2D": { "position": [0, 0] },
                                        "DontDestroy": { "key": "Camera" }, "CameraFollow": { "target": 1 } } },
            { "id": 3, "components": { "Name": { "name": "Wall" }, "Transform2D": { "position": [5, 5] } } }
        ]
    })";

    Registry registry;
    SceneMeta meta;
    CHECK(sceneFromString(registry, meta, mainJson).ok);
    CHECK(aliveCount(registry) == 3);

    const Entity player1 = findByKey(registry, "Player");
    const Entity camera1 = findByKey(registry, "Camera");
    CHECK(player1 != NULL_ENTITY && camera1 != NULL_ENTITY);
    CHECK(registry.getComponent<CameraFollow>(camera1).target == player1);

    registry.getComponent<Transform2D>(player1).setPosition({ 50.f, 50.f });
    registry.getComponent<RigidBody2D>(player1).velocity = { 5.f, 5.f };
    registry.getComponent<CameraFollow>(camera1).started = true;

    const auto donjonPath = dir / "donjon.scene";
    writeFile(donjonPath, R"({
        "format": "angeVault.scene", "version": 1, "name": "donjon",
        "entities": [
            { "id": 10, "components": { "Name": { "name": "Entree" }, "Transform2D": { "position": [100, 200] },
                                         "SpawnPoint": { "name": "entree" } } },
            { "id": 11, "components": { "Name": { "name": "Torch" }, "Transform2D": { "position": [1, 1] } } }
        ]
    })");

    // map -> donjon
    const SceneChangeResult r1 = changeScene(registry, "", donjonPath.string(), "entree");
    CHECK(r1.ok && r1.warnings.empty());
    CHECK(aliveCount(registry) == 4);
    CHECK(findByKey(registry, "Player") == player1);
    CHECK(findByKey(registry, "Camera") == camera1);
    CHECK(registry.getComponent<Transform2D>(player1).getPosition() == sf::Vector2f(100.f, 200.f));
    CHECK(registry.getComponent<RigidBody2D>(player1).velocity == sf::Vector2f(0.f, 0.f));
    CHECK(registry.getComponent<CameraFollow>(camera1).started == false);
    CHECK(registry.getComponent<CameraFollow>(camera1).target == player1);

    const SceneChangeResult r2 = changeScene(registry, "", donjonPath.string(), "absent");
    CHECK(r2.ok && r2.warnings.size() == 1);

    // donjon -> map
    const auto main2Path = dir / "main2.scene";
    writeFile(main2Path, R"({
        "format": "angeVault.scene", "version": 1, "name": "main2",
        "entities": [
            { "id": 20, "components": { "Name": { "name": "Sortie" }, "Transform2D": { "position": [7, 9] },
                                         "SpawnPoint": { "name": "sortie_donjon" } } }
        ]
    })");
    const SceneChangeResult r3 = changeScene(registry, "", main2Path.string(), "sortie_donjon");
    CHECK(r3.ok);
    CHECK(aliveCount(registry) == 3);
    CHECK(findByKey(registry, "Player") == player1);
    CHECK(registry.getComponent<Transform2D>(player1).getPosition() == sf::Vector2f(7.f, 9.f));

    std::filesystem::remove_all(dir);

    if (g_failures == 0) std::printf("test_scene_transition : OK\n");
    return g_failures == 0 ? 0 : 1;
}
