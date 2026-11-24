#include "systems/inventory_log_system.h"
#include "systems/pickup_system.h"

#include "components/collider2d.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"

#include <cstdio>
#include <sstream>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

static Entity box(Registry& r, sf::Vector2f pos, bool body) {
    Entity e = r.createEntity();
    r.addComponent<Transform2D>(e).setPosition(pos);
    r.addComponent<Collider2D>(e, Collider2D::makeBox({ 16.f, 16.f }));
    if (body) r.addComponent<RigidBody2D>(e).useGravity = false;
    return e;
}

static Entity item(Registry& r, sf::Vector2f pos, const std::string& name, int count, uint64_t id) {
    Entity e = box(r, pos, false);
    r.addComponent<Pickup>(e, Pickup{ name, count });
    r.addComponent<PersistentId>(e).value = id;
    return e;
}

// une frame : appui éventuel sur E, collisions, interactions puis ramassage
static void frame(InputManager& input, Registry& r, CollisionSystem& col, InteractionSystem& inter, PickupSystem& pick, bool pressE = false) {
    if (pressE) input.handleEvent(sf::Event::KeyPressed{ .code = sf::Keyboard::Key::E });
    input.update();
    col.onUpdate(r, 0.016f);
    inter.onUpdate(r, 0.016f);
    pick.onUpdate(r, 0.016f);
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
    PickupSystem pick(col, inter);
    int notified = 0;
    pick.addListener([&](Registry&, Entity, const std::string& name, int n) { if (name == "potion") notified += n; });

    Entity player = box(r, { 0.f, 0.f }, true);
    r.addComponent<Interactor>(player);
    Inventory& inv = r.addComponent<Inventory>(player);
    inv.capacity = 2;
    inv.maxStack = 5;

    // au contact
    Entity potion = item(r, { 4.f, 0.f }, "potion", 3, 101);
    Entity far = item(r, { 300.f, 0.f }, "potion", 1, 102);
    frame(input, r, col, inter, pick);
    CHECK(!r.isValid(potion));
    CHECK(r.isValid(far));
    CHECK(r.getComponent<Inventory>(player).count("potion") == 3);
    CHECK(notified == 3);
    CHECK(pick.collected(101) && !pick.collected(102));

    // interactable : attend l'action
    Entity key = item(r, { 4.f, 0.f }, "cle", 1, 103);
    r.addComponent<Interactable>(key);
    frame(input, r, col, inter, pick);
    CHECK(r.isValid(key));
    frame(input, r, col, inter, pick, true);
    CHECK(!r.isValid(key));
    CHECK(r.getComponent<Inventory>(player).has("cle"));

    // inventaire plein : le reste reste au sol
    Entity heavy = item(r, { 4.f, 0.f }, "potion", 6, 104);
    frame(input, r, col, inter, pick);
    CHECK(r.isValid(heavy));
    CHECK(r.getComponent<Pickup>(heavy).count == 4);
    CHECK(r.getComponent<Inventory>(player).count("potion") == 5);
    r.destroyEntity(heavy);
    r.flushPendingDestroys();

    // sans inventaire : rien
    Entity npc = box(r, { 400.f, 0.f }, true);
    Entity coin = item(r, { 404.f, 0.f }, "or", 1, 105);
    frame(input, r, col, inter, pick);
    CHECK(r.isValid(coin));
    CHECK(r.isValid(npc));

    // map rechargée : l'objet déjà pris ne revient pas
    Entity again = item(r, { 200.f, 200.f }, "potion", 3, 101);
    frame(input, r, col, inter, pick);
    CHECK(!r.isValid(again));

    // console : gains et pertes, contenu sur l'action
    {
        InventoryLogSystem log("interagir");
        std::ostringstream out;
        std::streambuf* old = std::cout.rdbuf(out.rdbuf());
        log.onUpdate(r, 0.f);   // première image : pas de gain affiché
        r.getComponent<Inventory>(player).capacity = 0;
        r.getComponent<Inventory>(player).add("epee", 1);
        r.getComponent<Inventory>(player).remove("cle", 1);
        log.onUpdate(r, 0.f);
        input.handleEvent(sf::Event::KeyPressed{ .code = sf::Keyboard::Key::E });
        input.update();
        log.onUpdate(r, 0.f);
        input.endFrame();
        std::cout.rdbuf(old);
        const std::string text = out.str();
        CHECK(text.find("+1 epee") != std::string::npos);
        CHECK(text.find("-1 cle") != std::string::npos);
        CHECK(text.find("+5 potion") == std::string::npos);
        CHECK(text.find("epee x1") != std::string::npos);
    }

    if (g_failures == 0) std::printf("test_pickup : OK\n");
    return g_failures == 0 ? 0 : 1;
}
