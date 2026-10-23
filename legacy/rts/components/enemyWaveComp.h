#pragma once

#include <SFML/System/Vector2.hpp>
#include <ecs/entity.h>
#include <cstdint>
#include <vector>

namespace ECS {
    struct EnemyWaveDirector {
        unsigned int wave = 0;
        float nextWaveTimer = 30.f;
        float spawnInterval = 30.f;
    };

    struct EnemySquad {
        unsigned int wave = 0;
        sf::Vector2f origin{ 0.f, 0.f };
        Entity target = NULL_ENTITY;
        float targetSearchTimer = 0.f;
        float interceptorSearchTimer = 0.f;
        std::vector<Entity> members;
    };

    struct EnemySquadMember {
        Entity squad = NULL_ENTITY;
        sf::Vector2f formationOffset{ 0.f, 0.f };
    };
}
