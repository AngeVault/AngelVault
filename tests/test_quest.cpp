#include "systems/chest_system.h"
#include "systems/quest_system.h"

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

static void frame(InputManager& input, Registry& r, CollisionSystem& col, InteractionSystem& inter, ChestSystem& chests, bool pressE) {
    if (pressE) frame(input, r, col, inter, chests, false);
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
    QuestSystem quests(inter);
    int changes = 0;
    quests.addListener([&](Registry&, const std::string&, QuestSystem::State) { ++changes; });

    Entity player = box(r, { 0.f, 0.f }, true);
    r.addComponent<Interactor>(player);
    r.addComponent<Inventory>(player).add("cle", 1);

    // coffre ouvert avant la quête : pas de récompense
    Entity early = box(r, { 4.f, 0.f }, false);
    r.addComponent<Chest>(early, Chest{ "", false, "", {}, 0.f });
    r.addComponent<Interactable>(early);
    r.addComponent<Inventory>(early).add("or", 5);
    r.addComponent<QuestObjective>(early, QuestObjective{ "epee", "potion_soin", 1 });
    frame(input, r, col, inter, chests, true);
    CHECK(quests.state("epee") == QuestSystem::State::None);
    CHECK(!r.getComponent<Inventory>(player).has("potion_soin"));
    r.destroyEntity(early);
    r.flushPendingDestroys();

    // le pnj donne la quête une seule fois
    Entity npc = box(r, { 4.f, 0.f }, false);
    r.addComponent<Interactable>(npc);
    r.addComponent<QuestGiver>(npc, QuestGiver{ "epee" });
    frame(input, r, col, inter, chests, true);
    CHECK(quests.state("epee") == QuestSystem::State::Active);
    frame(input, r, col, inter, chests, true);
    CHECK(changes == 1);
    r.destroyEntity(npc);
    r.flushPendingDestroys();

    // coffre verrouillé sans clé : la quête reste en cours
    r.getComponent<Inventory>(player).remove("cle", 1);
    Entity chest = box(r, { 4.f, 0.f }, false);
    r.addComponent<Chest>(chest, Chest{ "cle", false, "", {}, 0.f });
    r.addComponent<Interactable>(chest);
    r.addComponent<Inventory>(chest).add("epee", 1);
    r.addComponent<QuestObjective>(chest, QuestObjective{ "epee", "potion_soin", 1 });
    frame(input, r, col, inter, chests, true);
    CHECK(quests.state("epee") == QuestSystem::State::Active);
    CHECK(!r.getComponent<Inventory>(player).has("potion_soin"));

    // ouvert avec la clé : contenu + récompense, quête accomplie
    r.getComponent<Inventory>(player).add("cle", 1);
    frame(input, r, col, inter, chests, true);
    CHECK(quests.state("epee") == QuestSystem::State::Done);
    CHECK(r.getComponent<Inventory>(player).count("potion_soin") == 1);
    CHECK(r.getComponent<Inventory>(player).has("epee"));
    CHECK(changes == 2);

    if (g_failures == 0) std::printf("test_quest : OK\n");
    return g_failures == 0 ? 0 : 1;
}
