#pragma once

#include "../components/enemyWaveComp.h"
#include "../components/rtsPlayerState.h"
#include "../components/selectable.h"
#include <components/transform2d.h>
#include "../components/unitComp.h"
#include "../utils/entity_factory.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

class EnemyWaveSystem : public ISystem {
public:
    explicit EnemyWaveSystem(sf::Vector2f worldSize) : m_worldSize(worldSize) {
        priority = -4;
    }

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) return;

        std::vector<Entity> emptySquads;
        registry.view<ECS::EnemySquad>([&](Entity entity, ECS::EnemySquad& squad) {
            bool hasEnemy = false;
            for (Entity member : squad.members) {
                if (!registry.isValid(member) || !registry.hasComponent<ECS::Unit>(member)) continue;
                if (registry.getComponent<ECS::Unit>(member).team == ECS::Unit::Team::Enemy) {
                    hasEnemy = true;
                    break;
                }
            }
            if (!hasEnemy) emptySquads.push_back(entity);
        });
        for (Entity entity : emptySquads)
            if (registry.isValid(entity)) registry.destroyEntity(entity);

        std::size_t livingEnemies = 0;
        registry.view<ECS::Unit>([&](Entity, ECS::Unit& unit) {
            if (unit.team == ECS::Unit::Team::Enemy && unit.pv > 0) ++livingEnemies;
        });

        registry.view<ECS::EnemyWaveDirector>([&](Entity, ECS::EnemyWaveDirector& director) {
            if (livingEnemies > 0) {
                director.nextWaveTimer = director.spawnInterval;
                return;
            }
            director.nextWaveTimer -= dt;
            if (director.nextWaveTimer <= 0.f) {
                spawnWave(registry, director);
                director.nextWaveTimer = director.spawnInterval;
                registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
                    hud.message = "Vague " + std::to_string(director.wave) + " !";
                    hud.messageTimer = 2.5f;
                });
            }
        });
    }

private:
    sf::Vector2f m_worldSize;

    void spawnWave(Registry& registry, ECS::EnemyWaveDirector& director) {
        ++director.wave;
        constexpr std::size_t kStartingWaveSize = 7;
        constexpr std::size_t kMaxEnemiesPerWave = 240;
        constexpr float kEnemyGrowthPerWave = 1.22f;
        std::size_t totalCount = kStartingWaveSize;
        for (unsigned int wave = 1; wave < director.wave && totalCount < kMaxEnemiesPerWave; ++wave) {
            totalCount = std::min(kMaxEnemiesPerWave,
                static_cast<std::size_t>(std::ceil(totalCount * kEnemyGrowthPerWave)));
        }
        const std::size_t groupCount = director.wave >= 9 ? 4
            : (director.wave >= 5 ? 3 : (director.wave >= 2 ? 2 : 1));

        sf::Vector2f playerCenter{ 0.f, 0.f };
        std::size_t players = 0;
        registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit, Transform2D& transform) {
            if (unit.team != ECS::Unit::Team::Player || unit.pv <= 0) return;
            playerCenter += transform.getPosition();
            ++players;
        });
        if (players > 0) playerCenter /= static_cast<float>(players);
        else playerCenter = m_worldSize * 0.5f;

        // Les premières vagues restent lisibles, puis plusieurs escouades
        // attaquent depuis des côtés différents et tournent autour de la carte.
        const std::size_t firstSide = ((director.wave - 1) / 2) % 4;
        const std::array<std::size_t, 4> sideOrder{
            firstSide, (firstSide + 2) % 4, (firstSide + 1) % 4, (firstSide + 3) % 4
        };
        const std::size_t baseCount = totalCount / groupCount;
        const std::size_t extraCount = totalCount % groupCount;

        for (std::size_t group = 0; group < groupCount; ++group) {
            const std::size_t count = baseCount + (group < extraCount ? 1 : 0);
            const std::size_t columns = std::min<std::size_t>(4, count);
            const std::size_t rows = (count + columns - 1) / columns;
            const float edgeInset = 220.f + static_cast<float>(rows - 1) * 21.f;
            const float laneOffset = (static_cast<float>(group)
                - static_cast<float>(groupCount - 1) * 0.5f) * 240.f;

            sf::Vector2f center;
            sf::Vector2f forward;
            switch (sideOrder[group]) {
            case 0: // Est
                center = { m_worldSize.x - edgeInset,
                    std::clamp(playerCenter.y + laneOffset, 120.f, m_worldSize.y - 120.f) };
                forward = { -1.f, 0.f };
                break;
            case 1: // Nord
                center = { std::clamp(playerCenter.x + laneOffset, 120.f, m_worldSize.x - 120.f),
                    edgeInset };
                forward = { 0.f, 1.f };
                break;
            case 2: // Ouest
                center = { edgeInset,
                    std::clamp(playerCenter.y + laneOffset, 120.f, m_worldSize.y - 120.f) };
                forward = { 1.f, 0.f };
                break;
            default: // Sud
                center = { std::clamp(playerCenter.x + laneOffset, 120.f, m_worldSize.x - 120.f),
                    m_worldSize.y - edgeInset };
                forward = { 0.f, -1.f };
                break;
            }

            const Entity squadEntity = registry.createEntity();
            ECS::EnemySquad squad;
            squad.wave = director.wave;
            squad.origin = center;
            squad.members.reserve(count);
            const sf::Vector2f lateral{ -forward.y, forward.x };

            for (std::size_t row = 0, i = 0; row < rows; ++row) {
                const std::size_t rowCount = std::min(columns, count - i);
                const float firstLateral = -static_cast<float>(rowCount - 1) * 21.f;
                for (std::size_t column = 0; column < rowCount; ++column, ++i) {
                    const float lateralOffset = firstLateral + static_cast<float>(column) * 42.f;
                    const float depthOffset = (static_cast<float>(rows - 1) * 0.5f
                        - static_cast<float>(row)) * 42.f;
                    const sf::Vector2f formationOffset{ lateralOffset, depthOffset };
                    const sf::Vector2f position = center + lateral * lateralOffset + forward * depthOffset;
                    const bool backRankArcher = director.wave >= 2 && row + 1 == rows && rows > 1;
                    const bool extraArcher = (director.wave >= 12 && i % 3 == 0)
                        || (director.wave >= 6 && i % 4 == 0);
                    const ECS::Unit::Type type = backRankArcher || extraArcher
                        ? ECS::Unit::Type::Archer
                        : ((director.wave >= 2 && i % 3 == 1) ? ECS::Unit::Type::Melee : ECS::Unit::Type::Soldier);
                    const Entity member = Rts::createUnit(registry, type, ECS::Unit::Team::Enemy, position);
                    const unsigned int scalingStep = (director.wave - 1) / 3;
                    ECS::Unit& enemy = registry.getComponent<ECS::Unit>(member);
                    enemy.pvMax += static_cast<int>(scalingStep * 8);
                    enemy.pv = enemy.pvMax;
                    enemy.dmg += static_cast<int>(scalingStep);
                    enemy.speed += static_cast<float>(scalingStep) * 1.5f;
                    registry.addComponent<ECS::EnemySquadMember>(member,
                        ECS::EnemySquadMember{ squadEntity, formationOffset });
                    squad.members.push_back(member);
                }
            }
            registry.addComponent<ECS::EnemySquad>(squadEntity, squad);
        }
    }
};
