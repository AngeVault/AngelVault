#include "ecs/component_pool.h"
#include <cassert>
#include <cstdio>
#include <iostream>

struct Position { float x, y; };

void test_add_has_get() {
    ComponentPool<Position> pool;
    pool.add(0, Position{ 1.f, 1.f });
    pool.add(3, Position{ 3.f, 3.f }); // indices non contigus, exprès

    (pool.has(0));
    assert(pool.has(3));
    assert(!pool.has(1));           // jamais ajouté
    assert(pool.size() == 2);
    assert(pool.get(0).x == 1.f);
    assert(pool.get(3).x == 3.f);

    printf("test_add_has_get OK\n");
}

void test_remove_swap_pop() {
    ComponentPool<Position> pool;
    pool.add(0, Position{ 0.f, 0.f });
    pool.add(1, Position{ 1.f, 1.f });
    pool.add(2, Position{ 2.f, 2.f });
    // dense: [e0, e1, e2] — on retire celui du MILIEU

    pool.remove(1);

    assert(!pool.has(1));
    assert(pool.has(0));
    assert(pool.has(2));
    assert(pool.size() == 2);
    // l'entité 2 (l'ancien dernier) a dû prendre la place laissée par l'entité 1
    assert(pool.get(2).x == 2.f);              // toujours la bonne donnée
    assert(pool.denseToEntity()[1] == 2);      // la case dense 1 appartient maintenant à l'entité 2

    printf("test_remove_swap_pop OK\n");
}

void test_callbacks() {
    ComponentPool<Position> pool;
    int destroyCount = 0;
    float lastDestroyedX = -1.f;

	pool.onDestroy([&destroyCount, &lastDestroyedX](uint32_t entityIndex, Position& p) {
		destroyCount++;
		lastDestroyedX = p.x;
		});

    pool.add(5, Position{ 9.f, 9.f });
    pool.remove(5);

    assert(destroyCount == 1);

	assert(lastDestroyedX == 9.f);

    printf("test_callbacks OK\n");
}

void test_remove_last_element()
{
    ComponentPool<Position> pool;

    pool.add(0, Position{ 0,0 });
    pool.add(1, Position{ 1,1 });
    pool.add(2, Position{ 2,2 });
    pool.add(3, Position{ 3,3 });
    
    assert(pool.has(3));
    assert(pool.has(2));
    assert(pool.get(3).x == 3); 
    pool.remove(3); // 3 est déjà en dernière position du dense -> cas particulier (auto-swap)


    assert(!pool.has(3));
    assert(pool.size() == 3);
    assert(pool.denseToEntity()[2] == 2); // on verifie si aucun element as été swap donc que 2 n'as pas bougé
    printf("test_remoove_last OK \n");
}

int main() {
    test_add_has_get();
    test_remove_swap_pop();
	test_callbacks();
    test_remove_last_element();

    printf("Tous les tests sont passes.\n");
    return 0;
}