#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/rigid_body2d.h"
#include "components/unitComp.h"
#include "components/buildingComp.h"
#include "components/rtsPlayerState.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>

class UnitMovementSystem : public ISystem {
public:
    UnitMovementSystem() { priority = -1; } // juste avant physique

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) {
            registry.view<ECS::Unit, RigidBody2D>([&](Entity, ECS::Unit&, RigidBody2D& body) {
                body.velocity = { 0.f, 0.f };
            });
            return;
        }

        m_allUnits.clear();
        registry.view<ECS::Unit, Transform2D>(
            [&](Entity e, ECS::Unit& u, Transform2D& t) {
                m_allUnits.push_back({ e.index, t.getPosition(), u.team, u.movementGroup });
            });

        constexpr float kMinSeparation = 42.f;
        constexpr float kSeparationStr = 220.f;
        constexpr float kFormationSeparation = 36.f;
        m_spatialGrid.clear();
        for (size_t i = 0; i < m_allUnits.size(); ++i) {
            const int cellX = static_cast<int>(std::floor(m_allUnits[i].pos.x / kMinSeparation));
            const int cellY = static_cast<int>(std::floor(m_allUnits[i].pos.y / kMinSeparation));
            m_spatialGrid.push_back({ cellKey(cellX, cellY), i });
        }
        std::sort(m_spatialGrid.begin(), m_spatialGrid.end(), [](const CellEntry& a, const CellEntry& b) {
            return a.key < b.key;
        });

        registry.view<ECS::Unit, Transform2D, RigidBody2D>(
            [&](Entity e, ECS::Unit& u, Transform2D& t, RigidBody2D& rb) {
                rb.useGravity = false;
                rb.fixedRotation = true;

                sf::Vector2f pos = t.getPosition();
                sf::Vector2f desiredVelocity{ 0.f, 0.f };
                bool hasConstructionMove = false;

                if (u.type == ECS::Unit::Type::Builder && u.constructionTarget.index != UINT32_MAX) {
                    if (!registry.isValid(u.constructionTarget)
                        || !registry.hasComponent<ECS::Building>(u.constructionTarget)
                        || !registry.hasComponent<Transform2D>(u.constructionTarget)) {
                        u.constructionTarget = NULL_ENTITY;
                    }
                    else if (registry.getComponent<ECS::Building>(u.constructionTarget).constructionProgress >= 1.f) {
                        u.constructionTarget = NULL_ENTITY;
                    }
                    else {
                        hasConstructionMove = true;
                        const sf::Vector2f site = registry.getComponent<Transform2D>(u.constructionTarget).getPosition();
                        const sf::Vector2f offset = site - pos;
                        const float distance = std::hypot(offset.x, offset.y);
                        const float stopDistance = std::max(58.f,
                            registry.getComponent<ECS::Building>(u.constructionTarget).attackRadius);
                        if (distance > stopDistance)
                            desiredVelocity = (offset / distance) * u.speed;
                    }
                }

                if (!hasConstructionMove && u.hasDestination) {
                    const sf::Vector2f toDestination = u.destination - pos;
                    const float distance = std::hypot(toDestination.x, toDestination.y);
                    const float arrivalDistance = std::max(5.f, u.speed * std::min(dt, 0.05f));
                    if (u.movementGroup != 0 && distance < 34.f) {
                        // Garde une correction douce vers le slot même après l'arrivée.
                        // L'évitement peut donc écarter les soldats sans casser la ligne.
                        desiredVelocity = toDestination * 3.f;
                    }
                    else if (distance <= arrivalDistance && u.movementGroup == 0) {
                        u.hasDestination = false;
                    }
                    else {
                        desiredVelocity = (toDestination / distance) * u.speed;
                    }
                }

                sf::Vector2f separationVelocity{ 0.f, 0.f };
                const int cellX = static_cast<int>(std::floor(pos.x / kMinSeparation));
                const int cellY = static_cast<int>(std::floor(pos.y / kMinSeparation));
                for (int y = cellY - 1; y <= cellY + 1; ++y) {
                    for (int x = cellX - 1; x <= cellX + 1; ++x) {
                        const uint64_t key = cellKey(x, y);
                        auto entry = std::lower_bound(m_spatialGrid.begin(), m_spatialGrid.end(), key,
                            [](const CellEntry& candidate, uint64_t cell) {
                                return candidate.key < cell;
                            });
                        while (entry != m_spatialGrid.end() && entry->key == key) {
                            const auto& other = m_allUnits[entry->unitIndex];
                            ++entry;
                            if (other.idx == e.index || other.team != u.team) continue;
                            sf::Vector2f offset = pos - other.pos;
                            const float distanceSquared = offset.x * offset.x + offset.y * offset.y;
                            const bool sameFormation = u.movementGroup != 0
                                && u.movementGroup == other.movementGroup;
                            const float minSeparation = sameFormation
                                ? kFormationSeparation : kMinSeparation;
                            if (distanceSquared >= minSeparation * minSeparation) continue;
                            float separation = 0.f;
                            if (distanceSquared <= 1e-4f) {
                                offset = e.index < other.idx
                                    ? sf::Vector2f{ 1.f, 0.f } : sf::Vector2f{ -1.f, 0.f };
                            }
                            else {
                                separation = std::sqrt(distanceSquared);
                                offset /= separation;
                            }
                            const float push = (minSeparation - separation) / minSeparation;
                            separationVelocity += offset * push
                                * (sameFormation ? kSeparationStr * 0.7f : kSeparationStr);
                        }
                    }
                }

                sf::Vector2f vel = desiredVelocity + separationVelocity;
                const float speed = std::hypot(vel.x, vel.y);
                const float maxSpeed = u.speed * 1.5f;
                if (speed > maxSpeed && speed > 1e-4f)
                    vel = (vel / speed) * maxSpeed;
                rb.velocity = vel;
            });
    }

private:
    struct Info {
        uint32_t idx;
        sf::Vector2f pos;
        ECS::Unit::Team team;
        uint32_t movementGroup;
    };
    struct CellEntry { uint64_t key; size_t unitIndex; };
    std::vector<Info> m_allUnits;
    std::vector<CellEntry> m_spatialGrid;

    static uint64_t cellKey(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
            | static_cast<uint32_t>(y);
    }
};
