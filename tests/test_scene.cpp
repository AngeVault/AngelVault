// Tests de la sauvegarde : scènes (.scene) et projet (.angproj)
#include "scene/project.h"
#include "scene/scene_io.h"

#include "components/camera2d.h"
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/name.h"
#include "components/persistent_id.h"
#include "components/rigid_body2d.h"
#include "components/script_component.h"
#include "components/transform2d.h"
#include "ecs/behavior_factory.h"
#include "scene/prefab.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)
static bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }

struct TestBehavior : IBehavior {
    PROPERTY(float, speed, 250.f)
    PROPERTY(std::string, label, "a")
    void onUpdate(Entity, Registry&, float) override {}
};
REGISTER_BEHAVIOR(TestBehavior)

struct OtherBehavior : IBehavior {
    PROPERTY(int, count, 3)
    void onUpdate(Entity, Registry&, float) override {}
};
REGISTER_BEHAVIOR(OtherBehavior)

static Entity findByName(Registry& r, const std::string& name) {
    Entity found = NULL_ENTITY;
    r.forEachEntity([&](Entity e) {
        if (r.hasComponent<NameComponent>(e) && r.getComponent<NameComponent>(e).name == name) found = e;
    });
    return found;
}

int main() {
    // ---- Scène de départ : un joueur complet + une caméra qui le suit
    Registry a;
    Entity camera = a.createEntity();   // créée AVANT le joueur : le lien pointe « en avant »
    a.addComponent<NameComponent>(camera, "Camera");
    a.addComponent<Transform2D>(camera).setPosition({ 5.f, 6.f });
    auto& cam = a.addComponent<Camera2d>(camera);
    cam.zoom = 2.f;
    cam.setWorldBorder({ 0.f, 0.f }, { 3000.f, 2000.f });

    Entity player = a.createEntity();
    a.addComponent<NameComponent>(player, "Player");
    auto& t = a.addComponent<Transform2D>(player);
    t.setPosition({ 200.f, 100.f });
    t.setRotation(sf::degrees(45.f));
    t.setScale({ 2.f, 3.f });
    a.addComponent<Geometry>(player, Geometry::SpriteData{ "player", sf::IntRect({ 1, 2 }, { 32, 64 }), sf::Color(10, 20, 30, 40) }, 3);
    a.addComponent<RigidBody2D>(player).useGravity = false;
    a.addComponent<Collider2D>(player, Collider2D::makeCapsule(8.f, 30.f, true));
    auto& script = a.addComponent<Script>(player);
    script.add<TestBehavior>().speed = 99.f;
    script.get<TestBehavior>()->label = "héros";
    script.add<OtherBehavior>().count = 7;   // plusieurs comportements sur la même entité
    cam.setTarget(player);

    Entity box = a.createEntity();
    a.addComponent<Geometry>(box, Geometry::PolygonData{ { { 0, 0 }, { 10, 0 }, { 5, 8 } }, { sf::Color::Red, sf::Color::Blue, 2.f, "" } }, 0);

    SceneMeta meta;
    meta.name = "Test";
    meta.mapPath = "maps/test.map";
    meta.editor = { { "categories", { "Héros" } } };
    const std::string text = sceneToString(a, meta);
    const uint64_t playerId = a.getComponent<PersistentId>(player).value;
    CHECK(playerId != 0);

    // ---- Rechargement dans un registry où les index sont décalés (entités créées / détruites avant)
    Registry b;
    for (int i = 0; i < 5; ++i) b.destroyEntity(b.createEntity());
    b.flushPendingDestroys();
    b.createEntity();   // une entité déjà là, pas dans le fichier

    SceneMeta meta2;
    SceneLoadResult res = sceneFromString(b, meta2, text);
    CHECK(res.ok);
    CHECK(res.warnings.empty());
    CHECK(res.entities.size() == 3);
    CHECK(meta2.name == "Test" && meta2.mapPath == "maps/test.map");
    CHECK(meta2.editor["categories"][0] == "Héros");

    Entity p2 = findByName(b, "Player");
    Entity c2 = findByName(b, "Camera");
    CHECK(b.isValid(p2) && b.isValid(c2));
    CHECK(!(p2 == player));                                            // autre index qu'avant...
    CHECK(b.getComponent<Camera2d>(c2).targetLock == p2);              // ... mais le lien suit
    CHECK(b.getComponent<PersistentId>(p2).value == playerId);
    CHECK(near(b.getComponent<Camera2d>(c2).zoom, 2.f));
    CHECK(near(b.getComponent<Camera2d>(c2).worldMax.x, 3000.f));

    const Transform2D& t2 = b.getComponent<Transform2D>(p2);
    CHECK(near(t2.getPosition().x, 200.f) && near(t2.getPosition().y, 100.f));
    CHECK(near(t2.getRotation().asDegrees(), 45.f));
    CHECK(near(t2.getScale().y, 3.f));

    const Geometry& g2 = b.getComponent<Geometry>(p2);
    CHECK(g2.layer == 3);
    CHECK(std::holds_alternative<Geometry::SpriteData>(g2.data));
    const auto& sprite = std::get<Geometry::SpriteData>(g2.data);
    CHECK(sprite.textureId == "player" && sprite.rect.size.y == 64 && sprite.rect.position.x == 1);
    CHECK(sprite.tint == sf::Color(10, 20, 30, 40));

    CHECK(!b.getComponent<RigidBody2D>(p2).useGravity);
    const Collider2D& col = b.getComponent<Collider2D>(p2);
    CHECK(col.isTrigger && col.type() == Collider2D::ShapeType::Capsule);
    CHECK(near(std::get<Collider2D::Capsule>(col.shape).height, 30.f));

    const Script& s2 = b.getComponent<Script>(p2);
    CHECK(s2.size() == 2);
    CHECK(BehaviorFactory::get().nameOf(*s2.behaviors[0].behavior) == "TestBehavior");
    CHECK(s2.get<TestBehavior>() && near(s2.get<TestBehavior>()->speed, 99.f));
    CHECK(s2.get<TestBehavior>()->label == "héros");
    CHECK(s2.get<OtherBehavior>() && s2.get<OtherBehavior>()->count == 7);
    CHECK(BehaviorFactory::get().namesInFile("C:/ailleurs/TEST_SCENE.cpp").size() == 2);   // glisser le fichier

    // ---- Ancien format (un seul "behavior") toujours lisible
    {
        Registry old;
        SceneMeta m;
        const std::string oldText = R"({"format":"angeVault.scene","version":1,"entities":[{"id":5,"components":{
            "Script":{"behavior":"TestBehavior","properties":{"speed":12}}}}]})";
        SceneLoadResult r = sceneFromString(old, m, oldText);
        CHECK(r.ok && old.getComponent<Script>(r.entities[0]).get<TestBehavior>()->speed == 12.f);
    }

    // ---- Prefab : le joueur + sa caméra, instancié deux fois ailleurs
    {
        const std::vector<Entity> group = prefabGroup(a, player);
        CHECK(group.size() == 2 && group[0] == player && group[1] == camera);
        const Json prefab = prefabToJson(a, group, "Player");
        CHECK(prefabName(prefab) == "Player");
        Registry world;
        SceneLoadResult i1 = instantiatePrefab(world, prefab, sf::Vector2f{ 1000.f, 500.f });
        SceneLoadResult i2 = instantiatePrefab(world, prefab, sf::Vector2f{ 0.f, 0.f });
        CHECK(i1.ok && i2.ok && i1.entities.size() == 2);
        const Entity pl1 = i1.entities[0], cam1 = i1.entities[1], pl2 = i2.entities[0], cam2 = i2.entities[1];
        CHECK(world.getComponent<Camera2d>(cam1).targetLock == pl1);   // chaque caméra suit SON joueur
        CHECK(world.getComponent<Camera2d>(cam2).targetLock == pl2);
        CHECK(world.getComponent<PersistentId>(pl1).value != world.getComponent<PersistentId>(pl2).value);
        CHECK(world.getComponent<PersistentId>(pl1).value != playerId);
        CHECK(near(world.getComponent<Transform2D>(pl1).getPosition().x, 1000.f));
        // la caméra garde son décalage par rapport au joueur (5 - 200 = -195)
        CHECK(near(world.getComponent<Transform2D>(cam1).getPosition().x, 1000.f - 195.f));
        CHECK(world.getComponent<Script>(pl2).size() == 2);
        // prefab d'une caméra seule : le lien vers le joueur (hors prefab) devient null
        const Json camOnly = prefabToJson(a, { camera }, "Cam");
        CHECK(camOnly["entities"][0]["components"]["Camera2d"]["targetLock"].is_null());
    }

    Entity box2 = res.entities[2];
    const auto& poly = std::get<Geometry::PolygonData>(b.getComponent<Geometry>(box2).data);
    CHECK(poly.points.size() == 3 && near(poly.points[2].y, 8.f) && poly.style.fillColor == sf::Color::Red);

    // ---- Deuxième sauvegarde identique (les ids ne changent pas)
    Registry c;
    SceneMeta meta3;
    CHECK(sceneFromString(c, meta3, text).ok);
    CHECK(sceneToString(c, meta3) == text);

    // ---- Fichier abîmé : composant inconnu, cible disparue, JSON invalide
    Json broken = Json::parse(text);
    broken["entities"][0]["components"]["Inconnu"] = { { "x", 1 } };
    for (auto& e : broken["entities"]) if (e["id"] == playerId) e["id"] = 42;   // la caméra pointe vers un id absent
    Registry d;
    SceneMeta meta4;
    SceneLoadResult r4 = sceneFromJson(d, meta4, broken);
    CHECK(r4.ok);
    CHECK(r4.warnings.size() == 2);
    CHECK(d.getComponent<Camera2d>(findByName(d, "Camera")).targetLock == NULL_ENTITY);
    Registry e;
    CHECK(!sceneFromString(e, meta4, "{ pas du json").ok);
    CHECK(!sceneFromString(e, meta4, "{\"format\":\"autre\"}").ok);

    // ---- Projet : création, sauvegarde, relecture
    const auto dir = std::filesystem::temp_directory_path() / "angevault_test_projet";
    std::filesystem::remove_all(dir);
    Project proj;
    std::string err;
    CHECK(Project::create(dir.string(), "MonJeu", proj, &err));
    CHECK(std::filesystem::exists(dir / "MonJeu.angproj"));
    CHECK(std::filesystem::exists(dir / "scenes" / "main.scene"));
    proj.addScene("scenes/donjon.scene");
    proj.window.width = 1280;
    CHECK(proj.save(&err));
    Project again;
    CHECK(again.load((dir / "MonJeu.angproj").string(), &err));
    CHECK(again.name == "MonJeu" && again.scenes.size() == 2 && again.window.width == 1280);
    CHECK(again.relative(again.resolve("scenes/main.scene")) == "scenes/main.scene");
    Registry f;
    SceneMeta meta5;
    CHECK(loadScene(f, meta5, again.resolve(again.startScene)).ok);
    std::filesystem::remove_all(dir);

    if (g_failures) { std::printf("%d test(s) en échec\n", g_failures); return EXIT_FAILURE; }
    std::printf("test_scene : OK\n");
    return EXIT_SUCCESS;
}
