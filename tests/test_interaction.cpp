#include "systems/interaction_system.h"

#include "components/collider2d.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

struct CountBehavior : IBehavior {
    int* count = nullptr;
    void onUpdate(Entity, Registry&, float) override {}
    void onInteract(Entity, Registry&, Entity) override { ++*count; }
};

static Entity box(Registry& r, sf::Vector2f pos, bool body) {
    Entity e = r.createEntity();
    r.addComponent<Transform2D>(e).setPosition(pos);
    r.addComponent<Collider2D>(e, Collider2D::makeBox({ 16.f, 16.f }));
    if (body) r.addComponent<RigidBody2D>(e).useGravity = false;
    return e;
}

// une frame : appui éventuel sur E, collisions puis interactions
static void frame(InputManager& input, Registry& r, CollisionSystem& col, InteractionSystem& inter, bool pressE) {
    if (pressE) input.handleEvent(sf::Event::KeyPressed{ .code = sf::Keyboard::Key::E });
    input.update();
    col.onUpdate(r, 0.016f);
    inter.onUpdate(r, 0.016f);
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

    Entity player = box(r, { 0.f, 0.f }, true);
    r.addComponent<Interactor>(player);
    Entity near = box(r, { 10.f, 0.f }, false);
    r.addComponent<Interactable>(near);
    Entity far = box(r, { 20.f, 0.f }, false);
    r.addComponent<Interactable>(far);
    Entity away = box(r, { 500.f, 0.f }, false);
    r.addComponent<Interactable>(away);

    int count = 0;
    r.addComponent<Script>(near).add<CountBehavior>().count = &count;
    Entity last = NULL_ENTITY;
    int calls = 0;
    inter.addListener([&](Registry&, Entity who, Entity target) { CHECK(who == player); last = target; ++calls; });

    // contact sans appui : rien
    frame(input, r, col, inter, false);
    CHECK(calls == 0);

    // appui : seul le plus proche réagit, une seule fois
    frame(input, r, col, inter, true);
    CHECK(calls == 1);
    CHECK(last == near);
    CHECK(count == 1);

    // maintenu : pas de nouvelle interaction
    frame(input, r, col, inter, false);
    CHECK(calls == 1);

    // verrouillé : on passe au suivant
    r.getComponent<Interactable>(near).enabled = false;
    frame(input, r, col, inter, true);
    CHECK(calls == 2);
    CHECK(last == far);

    // hors de portée : rien
    r.getComponent<Transform2D>(player).setPosition({ -300.f, 0.f });
    col.forgetPositions();
    frame(input, r, col, inter, true);
    CHECK(calls == 2);

    InputManager::setCurrent(nullptr);
    if (g_failures == 0) std::printf("test_interaction : OK\n");
    return g_failures == 0 ? 0 : 1;
}
