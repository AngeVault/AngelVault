#pragma once

#include "../components/buildingComp.h"
#include "../components/rts_particle_visual.h"
#include "../components/unitComp.h"
#include <components/particle_emitter.h>
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <utils/particle_manager.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

// particules de combat : coups, morts, batiments touches ou detruits, conversions, soins
// on touche pas au combat, on regarde juste les pv et l'equipe d'une frame a l'autre
// les etincelles partent dans le sens du coup (de l'attaquant vers la victime)
// les effets sont crees apres les view sinon ca invalide le registry
class CombatFxSystem : public ISystem {
public:
    CombatFxSystem() { priority = 2; } // apres collision (1), avant ParticleSystem (3)

    void onUpdate(Registry& registry, float dt) override {
        if (dt <= 0.f) return; // pause / game over : rien ne bouge
        ++m_frame;
        m_events.clear();

        registry.view<ECS::Unit, Transform2D>([&](Entity e, ECS::Unit& unit, Transform2D& transform) {
            const std::uint64_t id = key(e);
            Tracked& tracked = m_tracked[id];
            const sf::Vector2f position = transform.getPosition();
            if (tracked.known && !tracked.deadHandled) {
                const bool onPlayer = unit.team == ECS::Unit::Team::Player;
                if (tracked.team != unit.team && unit.pv > 0) {
                    m_events.push_back(makeEvent(Kind::Conversion, position, 0, onPlayer, 0.f, id));
                }
                else if (unit.pv <= 0) {
                    m_events.push_back(makeEvent(Kind::UnitDeath, position, 0,
                        tracked.team == ECS::Unit::Team::Player, 0.f, id));
                    tracked.deadHandled = true;
                }
                else if (unit.pv < tracked.pv) {
                    m_events.push_back(makeEvent(Kind::UnitHit, position, tracked.pv - unit.pv, onPlayer, 0.f, id));
                }
                else if (unit.pv > tracked.pv) {
                    m_events.push_back(makeEvent(Kind::Heal, position, unit.pv - tracked.pv, onPlayer, 0.f, id));
                }
            }
            tracked.known = true;
            tracked.pv = unit.pv;
            tracked.team = unit.team;
            tracked.seen = m_frame;
        });

        registry.view<ECS::Building, Transform2D>([&](Entity e, ECS::Building& building, Transform2D& transform) {
            const std::uint64_t id = key(e);
            Tracked& tracked = m_tracked[id];
            const sf::Vector2f position = transform.getPosition();
            if (tracked.known && !tracked.deadHandled) {
                if (building.pv <= 0) {
                    m_events.push_back(makeEvent(Kind::BuildingDeath, position, 0, true, building.attackRadius, id));
                    tracked.deadHandled = true;
                }
                else if (building.pv < tracked.pv) {
                    m_events.push_back(makeEvent(Kind::BuildingHit, position, tracked.pv - building.pv, true,
                        building.attackRadius, id));
                }
            }
            tracked.known = true;
            tracked.pv = building.pv;
            tracked.seen = m_frame;
        });

        // qui frappe qui : pour orienter les etincelles
        m_attackers.clear();
        registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit, Transform2D& transform) {
            if (unit.pv <= 0 || unit.target.index == UINT32_MAX) return;
            m_attackers.push_back({ key(unit.target), transform.getPosition(), unit.type == ECS::Unit::Type::Archer });
        });
        for (Event& event : m_events) resolveDirection(event);

        // oublie les entites detruites
        for (auto it = m_tracked.begin(); it != m_tracked.end();) {
            if (it->second.seen != m_frame) it = m_tracked.erase(it);
            else ++it;
        }

        // limite d'emetteurs par frame, les morts et conversions passent toujours
        constexpr int kMaxEmittersPerFrame = 120;
        m_emittersThisFrame = 0;
        for (const Event& event : m_events) {
            const bool important = event.kind == Kind::UnitDeath || event.kind == Kind::BuildingDeath
                || event.kind == Kind::Conversion;
            if (!important && m_emittersThisFrame >= kMaxEmittersPerFrame) continue;
            spawn(registry, event);
        }
    }

private:
    enum class Kind { UnitHit, UnitDeath, BuildingHit, BuildingDeath, Conversion, Heal };

    struct Event {
        Kind kind;
        sf::Vector2f position;
        int amount;            // pv perdus ou rendus
        bool onPlayer;         // true si c'est une unite du joueur
        float radius;          // taille du batiment
        std::uint64_t victim;
        bool hasDirection = false;
        float angle = 0.f;     // degres, direction du coup (attaquant -> victime)
        bool ranged = false;   // coup d'archer
    };

    struct Attacker {
        std::uint64_t target;
        sf::Vector2f position;
        bool ranged;
    };

    struct Tracked {
        bool known = false;
        bool deadHandled = false;
        int pv = 0;
        ECS::Unit::Team team = ECS::Unit::Team::Player;
        std::uint64_t seen = 0;
        // derniere direction de coup, pour orienter l'effet de mort
        bool hasAngle = false;
        float lastAngle = 0.f;
        bool lastRanged = false;
        std::uint64_t angleFrame = 0;
    };

    std::unordered_map<std::uint64_t, Tracked> m_tracked;
    std::vector<Event> m_events;
    std::vector<Attacker> m_attackers;
    std::uint64_t m_frame = 0;
    int m_emittersThisFrame = 0;

    static std::uint64_t key(Entity e) {
        return (static_cast<std::uint64_t>(e.generation) << 32) | e.index;
    }

    static Event makeEvent(Kind kind, sf::Vector2f position, int amount, bool onPlayer, float radius,
        std::uint64_t victim) {
        Event event{};
        event.kind = kind;
        event.position = position;
        event.amount = amount;
        event.onPlayer = onPlayer;
        event.radius = radius;
        event.victim = victim;
        return event;
    }

    void resolveDirection(Event& event) {
        if (event.kind != Kind::UnitHit && event.kind != Kind::UnitDeath
            && event.kind != Kind::BuildingHit && event.kind != Kind::BuildingDeath) return;
        Tracked& tracked = m_tracked[event.victim];

        // l'attaquant le plus proche qui vise cette victime
        const Attacker* best = nullptr;
        float bestDistance = 1e12f;
        for (const Attacker& attacker : m_attackers) {
            if (attacker.target != event.victim) continue;
            const float dx = event.position.x - attacker.position.x;
            const float dy = event.position.y - attacker.position.y;
            const float distance = dx * dx + dy * dy;
            if (distance < 1.f || distance >= bestDistance) continue;
            best = &attacker;
            bestDistance = distance;
        }

        if (best) {
            event.hasDirection = true;
            event.angle = std::atan2(event.position.y - best->position.y,
                event.position.x - best->position.x) * 180.f / 3.14159265f;
            event.ranged = best->ranged;
            tracked.hasAngle = true;
            tracked.lastAngle = event.angle;
            tracked.lastRanged = event.ranged;
            tracked.angleFrame = m_frame;
        }
        else if (tracked.hasAngle && m_frame - tracked.angleFrame < 40) {
            // l'attaquant a deja lache sa cible (mort), on reprend le dernier coup
            event.hasDirection = true;
            event.angle = tracked.lastAngle;
            event.ranged = tracked.lastRanged;
        }
    }

    static RtsParticleVisual visual(sf::Color from, sf::Color to, float startRadius, float endRadius,
        float stretch = 0.f, float glow = 0.f, int segments = 8, bool soft = false) {
        RtsParticleVisual v;
        v.startColor = from;
        v.endColor = to;
        v.startRadius = startRadius;
        v.endRadius = endRadius;
        v.stretch = stretch;
        v.glow = glow;
        v.segments = segments;
        v.soft = soft;
        return v;
    }

    static ParticleEmitter emitter(float speedMin, float speedMax, float lifeMin, float lifeMax,
        sf::Vector2f acceleration, float spawnRadius) {
        ParticleEmitter e;
        e.spawnShape = spawnRadius > 0.f ? ParticleEmitter::SpawnShape::Circle
            : ParticleEmitter::SpawnShape::Point;
        e.spawnRadius = spawnRadius;
        e.speedMin = speedMin;
        e.speedMax = speedMax;
        e.lifetimeMin = lifeMin;
        e.lifetimeMax = lifeMax;
        e.acceleration = acceleration;
        return e;
    }

    // oriente les particules dans le sens du coup, sinon dans toutes les directions
    static ParticleEmitter aimed(ParticleEmitter e, const Event& event, float spread) {
        if (event.hasDirection) {
            e.angleMinDegrees = event.angle - spread;
            e.angleMaxDegrees = event.angle + spread;
        }
        return e;
    }

    static ParticleEmitter upward(ParticleEmitter e, float spread) {
        e.angleMinDegrees = -90.f - spread;
        e.angleMaxDegrees = -90.f + spread;
        return e;
    }

    void burst(Registry& registry, sf::Vector2f position, const ParticleEmitter& config,
        const RtsParticleVisual& look, int count) {
        if (count <= 0) return;
        const Entity entity = ParticleManager::emitBurst(registry, position, config,
            static_cast<std::uint32_t>(count));
        if (registry.isValid(entity))
            registry.addComponent<RtsParticleVisual>(entity, look);
        ++m_emittersThisFrame;
    }

    // flash : une seule grosse particule tres courte avec un halo
    void flash(Registry& registry, sf::Vector2f position, sf::Color from, sf::Color to, float radius,
        float life) {
        burst(registry, position, emitter(0.f, 0.f, life * 0.8f, life, {}, 0.f),
            visual(from, to, radius, radius * 0.2f, 0.f, 0.9f, 14, true), 1);
    }

    // nuage de poussiere / fumee qui grossit en s'effacant
    void puffs(Registry& registry, sf::Vector2f position, sf::Color from, sf::Color to, float startRadius,
        float endRadius, float speed, float life, float rise, float spawnRadius, int count) {
        burst(registry, position, emitter(speed * 0.3f, speed, life * 0.6f, life, { 0.f, -rise }, spawnRadius),
            visual(from, to, startRadius, endRadius, 0.f, 0.f, 12, true), count);
    }

    void spawn(Registry& registry, const Event& event) {
        const sf::Vector2f p = event.position;
        switch (event.kind) {
        case Kind::UnitHit: {
            const float power = std::clamp(static_cast<float>(event.amount) / 20.f, 0.6f, 2.f);
            const sf::Color hot = event.onPlayer ? sf::Color(255, 225, 215) : sf::Color(255, 250, 215);
            const sf::Color flashEnd = event.onPlayer ? sf::Color(255, 90, 80, 0) : sf::Color(255, 190, 80, 0);
            flash(registry, p, hot, flashEnd, event.ranged ? 10.f : 15.f, 0.11f);

            // etincelles allongees dans le sens du coup, jaunes sur un ennemi, rouges sur le joueur
            const RtsParticleVisual sparks = event.onPlayer
                ? visual({ 255, 130, 115, 255 }, { 160, 15, 40, 0 }, 2.7f, 0.6f, 0.035f)
                : visual({ 255, 245, 190, 255 }, { 255, 120, 30, 0 }, 2.7f, 0.6f, 0.035f);
            if (event.ranged)
                burst(registry, p, aimed(emitter(100.f, 220.f, 0.15f, 0.32f, { 0.f, 260.f }, 4.f), event, 30.f),
                    sparks, 3 + static_cast<int>(power * 2.f));
            else
                burst(registry, p, aimed(emitter(130.f, 290.f, 0.15f, 0.34f, { 0.f, 260.f }, 4.f), event, 42.f),
                    sparks, 5 + static_cast<int>(power * 3.f));

            // eclats de bois d'une fleche qui se plante
            if (event.ranged)
                burst(registry, p, aimed(emitter(60.f, 150.f, 0.25f, 0.45f, { 0.f, 320.f }, 3.f), event, 60.f),
                    visual({ 215, 175, 115, 255 }, { 110, 80, 50, 0 }, 1.8f, 0.6f, 0.03f), 3);
            break;
        }
        case Kind::UnitDeath: {
            const bool player = event.onPlayer;
            flash(registry, p, player ? sf::Color(255, 225, 235) : sf::Color(240, 240, 255),
                player ? sf::Color(255, 110, 150, 0) : sf::Color(150, 160, 220, 0), 27.f, 0.17f);

            const RtsParticleVisual sparks = player
                ? visual({ 255, 150, 170, 255 }, { 190, 30, 70, 0 }, 2.6f, 0.6f, 0.03f)
                : visual({ 235, 235, 250, 255 }, { 90, 90, 130, 0 }, 2.6f, 0.6f, 0.03f);
            burst(registry, p, aimed(emitter(90.f, 260.f, 0.3f, 0.6f, { 0.f, 200.f }, 5.f), event, 80.f),
                sparks, 12);

            puffs(registry, p, { 190, 185, 180, 190 }, { 120, 115, 110, 0 }, 6.f, 20.f, 45.f, 0.8f, 18.f, 8.f, 6);
            break;
        }
        case Kind::BuildingHit: {
            const float spread = std::max(12.f, event.radius * 0.4f);
            flash(registry, p, { 255, 240, 200 }, { 255, 180, 100, 0 }, 13.f, 0.1f);
            // debris de pierre / bois, lourds (retombent vite)
            burst(registry, p, aimed(emitter(50.f, 150.f, 0.3f, 0.6f, { 0.f, 320.f }, spread), event, 55.f),
                visual({ 205, 185, 150, 255 }, { 95, 80, 65, 0 }, 3.f, 1.f, 0.02f), 5);
            puffs(registry, p, { 170, 160, 150, 150 }, { 110, 105, 100, 0 }, 7.f, 16.f, 25.f, 0.7f, 10.f, spread, 2);
            break;
        }
        case Kind::BuildingDeath: {
            const float spread = std::max(20.f, event.radius * 0.6f);
            flash(registry, p, { 255, 235, 190 }, { 255, 150, 60, 0 }, 34.f, 0.22f);
            burst(registry, p, emitter(90.f, 320.f, 0.5f, 1.0f, { 0.f, 160.f }, spread * 0.8f),
                visual({ 255, 215, 120, 255 }, { 255, 70, 20, 0 }, 3.f, 0.6f, 0.04f), 22);
            burst(registry, p, emitter(50.f, 200.f, 0.7f, 1.3f, { 0.f, 380.f }, spread),
                visual({ 180, 160, 135, 255 }, { 70, 60, 50, 0 }, 4.f, 2.f, 0.f, 0.f, 6), 14);
            puffs(registry, p, { 95, 90, 88, 210 }, { 40, 38, 38, 0 }, 12.f, 38.f, 55.f, 1.8f, 30.f, spread, 12);
            break;
        }
        case Kind::Conversion: {
            // l'ennemi vaincu change de camp : flash + etincelles roses qui montent
            flash(registry, p, { 255, 240, 255 }, { 255, 140, 220, 0 }, 20.f, 0.2f);
            burst(registry, p, upward(emitter(40.f, 140.f, 0.6f, 1.0f, { 0.f, -40.f }, 8.f), 55.f),
                visual({ 255, 235, 250, 255 }, { 255, 110, 200, 0 }, 2.6f, 0.6f, 0.05f, 0.5f), 16);
            break;
        }
        case Kind::Heal: {
            burst(registry, p, upward(emitter(12.f, 35.f, 0.55f, 0.9f, { 0.f, -25.f }, 12.f), 30.f),
                visual({ 190, 255, 215, 240 }, { 60, 215, 120, 0 }, 3.f, 0.8f, 0.f, 0.8f), 5);
            break;
        }
        }
    }
};
