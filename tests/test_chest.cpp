#include "systems/chest_system.h"

#include "components/collider2d.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

static Entity box(Registry& r, sf::Vector2f pos, bool body) {
    Entity e = r.createEntity();
    r.addComponent<Transform2D>(e).setPosition(pos);
    r.addComponent<Collider2D>(e, Collider2D::makeBox({ 16.f, 16.f }));
    if (body) r.addComponent<RigidBody2D>(e).useGravity = false;
    return e;
}

// coffre fermé ligne 0 d'une planche, ouvert ligne 1
static Entity chest(Registry& r, sf::Vector2f pos, uint64_t id, const std::string& key = "") {
    Entity e = box(r, pos, false);
    r.addComponent<Chest>(e, Chest{ key, false, "", { 0.f, 94.f }, 0.f });
    r.addComponent<Interactable>(e);
    r.addComponent<Geometry>(e, Geometry::SpriteData{ "chest", sf::IntRect({ 102, 0 }, { 102, 94 }), sf::Color::White }, 0);
    r.addComponent<PersistentId>(e).value = id;
    Inventory& inv = r.addComponent<Inventory>(e);
    inv.add("potion", 4);
    inv.add("or", 50);
    return e;
}

static int rectY(Registry& r, Entity e) { return std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.position.y; }

static void frame(InputManager& input, Registry& r, CollisionSystem& col, InteractionSystem& inter, ChestSystem& chests, bool pressE) {
    if (pressE) frame(input, r, col, inter, chests, false);   // touche relâchée avant un nouvel appui
    if (pressE) input.handleEvent(sf::Event::KeyPressed{ .code = sf::Keyboard::Key::E });
    input.update();
    col.onUpdate(r, 0.016f);
    inter.onUpdate(r, 0.016f);
    chests.onUpdate(r, 0.016f);
    r.flushPendingDestroys();
    input.endFrame();
    if (pressE) input.handleEvent(sf::Event::KeyReleased{ .code = sf::Keyboard::Key::E });
}

int main() {
    InputManager input;
    input.addAction("interagir").bindings = { { InputBinding::Source::Key, int(sf::Keyboard::Key::E), 1.f } };
    InputManager::setCurrent(&input);

    Registry r;
    CollisionSystem col;
    InteractionSystem inter(col);
    ChestSystem chests(inter);
    int opened = 0;
    chests.addListener([&](Registry&, Entity, Entity) { ++opened; });

    Entity player = box(r, { 0.f, 0.f }, true);
    r.addComponent<Interactor>(player);
    Inventory& inv = r.addComponent<Inventory>(player);
    inv.capacity = 2;
    inv.maxStack = 10;

    // le contact seul n'ouvre pas
    Entity c = chest(r, { 4.f, 0.f }, 201);
    frame(input, r, col, inter, chests, false);
    CHECK(!r.getComponent<Chest>(c).opened);

    // ouverture : sprite décalé une fois, contenu pris, le reste dedans car l'inventaire est trop petit
    frame(input, r, col, inter, chests, true);
    CHECK(r.getComponent<Chest>(c).opened);
    CHECK(rectY(r, c) == 94);
    CHECK(opened == 1);
    CHECK(r.getComponent<Inventory>(player).count("potion") == 4);
    CHECK(r.getComponent<Inventory>(player).count("or") == 10);
    CHECK(r.getComponent<Inventory>(c).count("or") == 40);
    CHECK(r.getComponent<Interactable>(c).enabled);

    // de la place : on revient chercher la suite, sans redécaler le sprite
    r.getComponent<Inventory>(player).maxStack = 0;
    frame(input, r, col, inter, chests, true);
    CHECK(r.getComponent<Inventory>(player).count("or") == 50);
    CHECK(r.getComponent<Inventory>(c).stacks.empty());
    CHECK(rectY(r, c) == 94);
    CHECK(opened == 1);
    CHECK(!r.getComponent<Interactable>(c).enabled);
    r.destroyEntity(c);
    r.flushPendingDestroys();

    // verrouillé : rien sans la clé, la clé est consommée
    Entity locked = chest(r, { 4.f, 0.f }, 202, "cle");
    frame(input, r, col, inter, chests, true);
    CHECK(!r.getComponent<Chest>(locked).opened);
    r.getComponent<Inventory>(player).capacity = 0;
    r.getComponent<Inventory>(player).add("cle", 1);
    frame(input, r, col, inter, chests, true);
    CHECK(r.getComponent<Chest>(locked).opened);
    CHECK(!r.getComponent<Inventory>(player).has("cle"));
    r.destroyEntity(locked);
    r.flushPendingDestroys();

    // map rechargée : le coffre vidé revient ouvert et vide
    Entity again = chest(r, { 300.f, 0.f }, 201);
    frame(input, r, col, inter, chests, false);
    CHECK(r.getComponent<Chest>(again).opened);
    CHECK(rectY(r, again) == 94);
    CHECK(r.getComponent<Inventory>(again).stacks.empty());
    CHECK(!r.getComponent<Interactable>(again).enabled);

    // animation : s'écrase avant d'afficher le sprite ouvert, puis revient à sa taille
    Entity anim = chest(r, { 4.f, 0.f }, 203);
    r.getComponent<Chest>(anim).openTime = 0.4f;
    r.getComponent<Transform2D>(anim).setScale({ 2.f, 2.f });
    frame(input, r, col, inter, chests, true);
    CHECK(r.getComponent<Chest>(anim).opened);
    CHECK(rectY(r, anim) == 0);
    CHECK(r.getComponent<Transform2D>(anim).getScale().y < 2.f);
    for (int i = 0; i < 40; ++i) frame(input, r, col, inter, chests, false);
    CHECK(rectY(r, anim) == 94);
    CHECK(r.getComponent<Transform2D>(anim).getScale() == sf::Vector2f(2.f, 2.f));

    if (g_failures == 0) std::printf("test_chest : OK\n");
    return g_failures == 0 ? 0 : 1;
}
