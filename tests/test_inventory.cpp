#include "components/inventory.h"
#include "scene/component_serializer.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

int main() {
    // empilement puis nouvelle case quand la pile est pleine
    {
        Inventory inv;
        inv.capacity = 3;
        inv.maxStack = 5;
        CHECK(inv.add("potion", 3) == 0);
        CHECK(inv.add("potion", 4) == 0);
        CHECK(inv.stacks.size() == 2);
        CHECK(inv.stacks[0].count == 5 && inv.stacks[1].count == 2);
        CHECK(inv.count("potion") == 7);
        CHECK(inv.has("potion", 7) && !inv.has("potion", 8));
    }

    // inventaire plein : le reste est renvoyé
    {
        Inventory inv;
        inv.capacity = 2;
        inv.maxStack = 5;
        inv.add("cle", 1);
        CHECK(inv.space("potion") == 5);
        CHECK(inv.add("potion", 8) == 3);
        CHECK(inv.count("potion") == 5);
        CHECK(inv.space("cle") == 4);
        CHECK(inv.add("epee", 1) == 1);
    }

    // retrait : les cases vides disparaissent
    {
        Inventory inv;
        inv.maxStack = 5;
        inv.add("potion", 7);
        inv.add("cle", 1);
        CHECK(inv.remove("potion", 3) == 3);
        CHECK(inv.count("potion") == 4);
        CHECK(inv.remove("potion", 10) == 4);
        CHECK(!inv.has("potion"));
        CHECK(inv.stacks.size() == 1 && inv.stacks[0].item == "cle");
        CHECK(inv.remove("rien", 1) == 0);
    }

    // sans limite et entrées invalides
    {
        Inventory inv;
        inv.capacity = 0;
        inv.maxStack = 0;
        CHECK(inv.add("or", 1000) == 0);
        CHECK(inv.stacks.size() == 1);
        CHECK(inv.add("", 3) == 3);
        CHECK(inv.add("or", -2) == 0);
        CHECK(inv.count("or") == 1000);
    }

    // sauvegarde puis relecture
    {
        Registry r;
        Entity e = r.createEntity();
        Inventory& inv = r.addComponent<Inventory>(e);
        inv.capacity = 8;
        inv.add("potion", 2);
        inv.add("cle", 1);
        const ComponentSerializer* s = SerializerRegistry::get().find("Inventory");
        CHECK(s != nullptr);
        if (s) {
            const Json j = s->save(r, e, SaveContext{});
            Entity f = r.createEntity();
            s->load(r, f, j, LoadContext{});
            const Inventory& back = r.getComponent<Inventory>(f);
            CHECK(back.capacity == 8);
            CHECK(back.stacks.size() == 2);
            CHECK(back.count("potion") == 2 && back.count("cle") == 1);
        }
    }

    if (g_failures == 0) std::printf("test_inventory : OK\n");
    return g_failures == 0 ? 0 : 1;
}
