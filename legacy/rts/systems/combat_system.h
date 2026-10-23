#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "components/buildingComp.h"
#include "components/selectable.h"
#include "components/unitVisuals.h"
#include "components/projectileComp.h"
#include "components/collider2d.h"
#include "components/rigid_body2d.h"
#include "components/geometry.h"
#include "components/enemyWaveComp.h"
#include "components/rtsPlayerState.h"
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdlib>

class CombatSystem : public ISystem {
public:
    static constexpr int kConversionChancePercent = 30;

    CombatSystem() { priority = -3; }

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) return;

        std::vector<PendingShot> shots;

        registry.view<ECS::Unit>([&](Entity, ECS::Unit& u) {
            if (u.attackTimer > 0.f) u.attackTimer -= dt;
            if (u.targetSearchTimer > 0.f)
                u.targetSearchTimer = std::max(0.f, u.targetSearchTimer - dt);
            });
        registry.view<ECS::EnemySquad>([&](Entity, ECS::EnemySquad& squad) {
            if (squad.targetSearchTimer > 0.f)
                squad.targetSearchTimer = std::max(0.f, squad.targetSearchTimer - dt);
            if (squad.interceptorSearchTimer > 0.f)
                squad.interceptorSearchTimer = std::max(0.f, squad.interceptorSearchTimer - dt);
        });

        registry.view<ECS::Unit, Transform2D>([&](Entity e, ECS::Unit& u, Transform2D& t) {
            if (u.pv <= 0) return;
            if (!u.canAttack) return;

            if (u.target.index != UINT32_MAX) {
                const bool valid = isHostileTarget(registry, u.team, u.target);
                if (!valid) {
                    u.target = NULL_ENTITY;
                    u.hasDestination = false;
                }
            }

            // Enemy waves should engage a player formation that blocks their route,
            // even when their squad already had a building as its target.
            if (u.team == ECS::Unit::Team::Enemy
                && u.target.index != UINT32_MAX
                && registry.hasComponent<ECS::Building>(u.target)) {
                Entity squadEntity = NULL_ENTITY;
                if (registry.hasComponent<ECS::EnemySquadMember>(e)) {
                    squadEntity = registry.getComponent<ECS::EnemySquadMember>(e).squad;
                    if (registry.isValid(squadEntity)
                        && registry.hasComponent<ECS::EnemySquad>(squadEntity)) {
                        const Entity squadTarget = registry.getComponent<ECS::EnemySquad>(squadEntity).target;
                        if (isHostileTarget(registry, u.team, squadTarget)
                            && registry.hasComponent<ECS::Unit>(squadTarget))
                            u.target = squadTarget;
                    }
                }
                if (registry.hasComponent<ECS::Building>(u.target)) {
                    bool shouldSearchInterceptor = true;
                    ECS::EnemySquad* squad = nullptr;
                    if (registry.isValid(squadEntity)
                        && registry.hasComponent<ECS::EnemySquad>(squadEntity)) {
                        squad = &registry.getComponent<ECS::EnemySquad>(squadEntity);
                        shouldSearchInterceptor = squad->interceptorSearchTimer <= 0.f;
                        if (shouldSearchInterceptor) squad->interceptorSearchTimer = 0.45f;
                    }
                    if (shouldSearchInterceptor) {
                        const Entity interceptor = findNearestPlayerUnit(registry, t.getPosition(), 300.f);
                        if (interceptor.index != UINT32_MAX) {
                            u.target = interceptor;
                            if (squad) squad->target = interceptor;
                        }
                    }
                }
            }

            if (u.target.index == UINT32_MAX) {
                if (u.team == ECS::Unit::Team::Enemy) {
                    if (registry.hasComponent<ECS::EnemySquadMember>(e)) {
                        const Entity squadEntity = registry.getComponent<ECS::EnemySquadMember>(e).squad;
                        if (registry.isValid(squadEntity) && registry.hasComponent<ECS::EnemySquad>(squadEntity)) {
                            ECS::EnemySquad& squad = registry.getComponent<ECS::EnemySquad>(squadEntity);
                            const bool squadTargetValid = isHostileTarget(registry, u.team, squad.target);
                            if (!squadTargetValid && squad.targetSearchTimer <= 0.f) {
                                squad.target = findNearestEnemy(registry, u, t.getPosition(), FLT_MAX);
                                squad.targetSearchTimer = 0.45f;
                            }
                            u.target = isHostileTarget(registry, u.team, squad.target)
                                ? squad.target : NULL_ENTITY;
                        }
                    }
                    else if (u.targetSearchTimer <= 0.f) {
                        u.target = findNearestEnemy(registry, u, t.getPosition(), FLT_MAX);
                        u.targetSearchTimer = 0.45f;
                    }
                }
                else if (u.chargeOrder) {
                    if (u.targetSearchTimer <= 0.f) {
                        u.target = findNearestEnemy(registry, u, t.getPosition(), FLT_MAX);
                        u.targetSearchTimer = 0.35f;
                        if (u.target.index == UINT32_MAX) {
                            u.chargeOrder = false;
                            u.hasDestination = false;
                        }
                    }
                }
                // Une troupe en formation garde son ordre de déplacement et ne
                // relance pas une recherche complète de cible à chaque intervalle.
                // Les unités hors formation restent capables de se défendre au repos.
                else if (u.movementGroup == 0 && !u.hasDestination) {
                    if (u.targetSearchTimer <= 0.f) {
                        u.target = findNearestEnemy(registry, u, t.getPosition(), u.attackRange);
                        u.targetSearchTimer = 0.35f;
                    }
                }
            }

            if (u.target.index == UINT32_MAX) return;
            if (u.team == ECS::Unit::Team::Player && u.movementGroup != 0)
                u.movementGroup = 0;

            const Entity tgt = u.target;
            if (!registry.isValid(tgt) || !registry.hasComponent<Transform2D>(tgt)) {
                u.target = NULL_ENTITY;
                u.hasDestination = false;
                return;
            }
            sf::Vector2f tp = registry.getComponent<Transform2D>(tgt).getPosition();
            float dx = tp.x - t.getPosition().x;
            float dy = tp.y - t.getPosition().y;
            float dist = std::hypot(dx, dy);
            float targetRadius = 0.f;
            if (registry.hasComponent<ECS::Building>(tgt))
                targetRadius = registry.getComponent<ECS::Building>(tgt).attackRadius;
            const float attackReach = u.attackRange + targetRadius;

            sf::Vector2f formationPosition = tp;
            bool hasFormationPosition = false;
            if (u.team == ECS::Unit::Team::Enemy
                && registry.hasComponent<ECS::EnemySquadMember>(e)) {
                const ECS::EnemySquadMember& member = registry.getComponent<ECS::EnemySquadMember>(e);
                if (registry.isValid(member.squad) && registry.hasComponent<ECS::EnemySquad>(member.squad)) {
                    const ECS::EnemySquad& squad = registry.getComponent<ECS::EnemySquad>(member.squad);
                    sf::Vector2f forward = tp - squad.origin;
                    const float length = std::hypot(forward.x, forward.y);
                    if (length > 1.f) forward /= length;
                    else forward = { 1.f, 0.f };
                    const sf::Vector2f lateral{ -forward.y, forward.x };
                    const float depth = targetRadius + (u.type == ECS::Unit::Type::Archer
                        ? std::min(u.attackRange * 0.72f, 150.f)
                        : std::min(u.attackRange * 0.7f,
                            22.f + std::max(0.f, -member.formationOffset.y) * 0.25f));
                    float lateralOffset = member.formationOffset.x;
                    if (u.type != ECS::Unit::Type::Archer) {
                        const float remainingRangeSq = std::max(0.f,
                            attackReach * attackReach - depth * depth);
                        const float maxLateral = std::sqrt(remainingRangeSq) * 0.8f;
                        lateralOffset = std::clamp(lateralOffset, -maxLateral, maxLateral);
                    }
                    formationPosition = tp - forward * depth
                        + lateral * lateralOffset;
                    hasFormationPosition = true;
                }
            }

            if (dist <= attackReach) {
                const float slotDistance = hasFormationPosition
                    ? std::hypot(formationPosition.x - t.getPosition().x,
                        formationPosition.y - t.getPosition().y) : 0.f;
                if (hasFormationPosition && slotDistance > 12.f) {
                    u.destination = formationPosition;
                    u.hasDestination = true;
                }
                else {
                    u.hasDestination = false;
                }
                if (u.attackTimer <= 0.f) {
                    u.attackTimer = u.attackCooldown;
                    const int damage = ECS::effectiveDamage(u);
                    if (u.type == ECS::Unit::Type::Archer)
                        shots.push_back({ t.getPosition(), u.target, damage, u.team });
                    else if (registry.hasComponent<ECS::Unit>(tgt))
                        registry.getComponent<ECS::Unit>(tgt).pv -= damage;
                    else if (registry.hasComponent<ECS::Building>(tgt))
                        registry.getComponent<ECS::Building>(tgt).pv -= damage;
                }
            }
            else {
                u.destination = hasFormationPosition ? formationPosition : tp;
                u.hasDestination = true;
            }
            });


        for (auto& s : shots)
            spawnProjectile(registry, s.from, s.target, s.damage, s.ownerTeam);

        registry.view<ECS::Unit>([&](Entity e, ECS::Unit& u) {
            if (u.pv <= 0) {
                if (u.team == ECS::Unit::Team::Enemy) {
                    const int scoreValue = u.type == ECS::Unit::Type::Archer ? 150
                        : (u.type == ECS::Unit::Type::Melee ? 125 : 100);
                    registry.view<ECS::RtsScore>([&](Entity, ECS::RtsScore& score) {
                        score.value += scoreValue;
                    });
                    if (std::rand() % 100 < kConversionChancePercent) {
                        convertToPlayer(registry, e, u);
                        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
                            hud.message = "Ennemi converti ! Chance : "
                                + std::to_string(kConversionChancePercent) + "%";
                            hud.messageTimer = 2.f;
                        });
                    }
                    else {
                        registry.destroyEntity(e);
                    }
                }
                else {
                    registry.destroyEntity(e);
                }
            }
            });

        std::vector<Entity> destroyedBuildings;
        registry.view<ECS::Building>([&](Entity entity, ECS::Building& building) {
            if (building.pv <= 0) destroyedBuildings.push_back(entity);
        });
        for (const Entity entity : destroyedBuildings)
            if (registry.isValid(entity)) registry.destroyEntity(entity);
    }

private:
    struct PendingShot {
        sf::Vector2f from;
        Entity target;
        int damage;
        ECS::Unit::Team ownerTeam;
    };

    Entity findNearestEnemy(Registry& registry, const ECS::Unit& self, sf::Vector2f pos, float maxRange) {
        Entity best = NULL_ENTITY;
        float bestSq = FLT_MAX;
        float maxSq = maxRange * maxRange;
        registry.view<ECS::Unit, Transform2D>([&](Entity e, ECS::Unit& other, Transform2D& t) {
            if (other.team == self.team) return;
            if (other.pv <= 0) return;
            float dx = t.getPosition().x - pos.x;
            float dy = t.getPosition().y - pos.y;
            float dSq = dx * dx + dy * dy;
            if (dSq <= maxSq && dSq < bestSq) { bestSq = dSq; best = e; }
            });
        if (self.team == ECS::Unit::Team::Enemy) {
            registry.view<ECS::Building, Transform2D>([&](Entity e, ECS::Building& building, Transform2D& t) {
                if (building.pv <= 0) return;
                const float dx = t.getPosition().x - pos.x;
                const float dy = t.getPosition().y - pos.y;
                const float dSq = dx * dx + dy * dy;
                if (dSq <= maxSq && dSq < bestSq) {
                    bestSq = dSq;
                    best = e;
                }
            });
        }
        return best;
    }

    Entity findNearestPlayerUnit(Registry& registry, sf::Vector2f pos, float maxRange) {
        Entity best = NULL_ENTITY;
        float bestSq = maxRange * maxRange;
        registry.view<ECS::Unit, Transform2D>([&](Entity entity, ECS::Unit& unit,
            Transform2D& transform) {
            if (unit.team != ECS::Unit::Team::Player || unit.pv <= 0) return;
            const sf::Vector2f offset = transform.getPosition() - pos;
            const float distanceSq = offset.x * offset.x + offset.y * offset.y;
            if (distanceSq < bestSq) {
                bestSq = distanceSq;
                best = entity;
            }
        });
        return best;
    }

    static bool isHostileTarget(Registry& registry, ECS::Unit::Team team, Entity target) {
        if (!registry.isValid(target)) return false;
        if (registry.hasComponent<ECS::Unit>(target)) {
            const ECS::Unit& unit = registry.getComponent<ECS::Unit>(target);
            return unit.team != team && unit.pv > 0;
        }
        return team == ECS::Unit::Team::Enemy
            && registry.hasComponent<ECS::Building>(target)
            && registry.getComponent<ECS::Building>(target).pv > 0;
    }

    // Un ennemi tué rejoint l'équipe du joueur avec peu de vie
    void convertToPlayer(Registry& registry, Entity e, ECS::Unit& u) {
        u.team = ECS::Unit::Team::Player;
        u.pv = std::max(1, u.pvMax / 4); // peu de vie
        u.target = NULL_ENTITY;
        u.chargeOrder = false;
        u.hasDestination = false;
        if (registry.hasComponent<ECS::Selectable>(e)) {
            ECS::Selectable& selectable = registry.getComponent<ECS::Selectable>(e);
            selectable.playerOwned = true;
            selectable.selected = false;
        }

        if (registry.hasComponent<Geometry>(e)) {
            Geometry& geo = registry.getComponent<Geometry>(e);
            if (auto* sd = std::get_if<Geometry::SpriteData>(&geo.data))
                sd->tint = unitTint(u.type, u.team); // retrouve sa couleur
        }
    }

    void spawnProjectile(Registry& registry, sf::Vector2f from, Entity target, int damage,
        ECS::Unit::Team ownerTeam) {
        if (!registry.isValid(target) || !registry.hasComponent<Transform2D>(target)) return;
        const sf::Vector2f offset = registry.getComponent<Transform2D>(target).getPosition() - from;
        const float distance = std::hypot(offset.x, offset.y);
        if (distance <= 1e-4f) return;

        Entity e = registry.createEntity();

        Transform2D t;
        t.setPosition(from);
        registry.addComponent<Transform2D>(e, t);

        Geometry geo;
        geo.layer = 2;
        geo.data = Geometry::CircleData{ 6.f, { sf::Color(255, 220, 90), sf::Color::Transparent, 0.f, "" } };
        registry.addComponent<Geometry>(e, geo);

        Projectile p;
        p.dir = offset / distance;
        p.damage = damage;
        p.ownerTeam = ownerTeam;
        registry.addComponent<Projectile>(e, p);
        registry.addComponent<Collider2D>(e, Collider2D::makeCircle(6.f, true));
        RigidBody2D body;
        body.useGravity = false;
        body.fixedRotation = true;
        registry.addComponent<RigidBody2D>(e, body);
    }
};
