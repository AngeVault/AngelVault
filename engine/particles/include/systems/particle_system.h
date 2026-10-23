#pragma once

#include <ecs/registry.h>
#include <ecs/system.h>
#include <components/particle.h>
#include <components/particle_emitter.h>
#include <components/transform2d.h>
#include <SFML/System/Angle.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

// Produit les entités particules puis simule leur mouvement et leur durée de vie.
// Les nouvelles entités sont créées après l'itération des émetteurs afin de ne pas
// invalider les vues du Registry qui contiennent Transform2D.
class ParticleSystem : public ISystem {
public:
    ParticleSystem() { priority = 3; }

    void onUpdate(Registry& registry, float dt) override {
        dt = std::max(0.f, dt);
        m_batches.clear();

        registry.view<Transform2D, ParticleEmitter, ParticleEmitterRuntime>(
            [&](Entity emitterEntity, Transform2D& transform, ParticleEmitter& emitter,
                ParticleEmitterRuntime& runtime) {
                const float previousElapsed = runtime.elapsed;
                runtime.elapsed += dt;

                const bool withinDuration = emitter.duration < 0.f
                    || previousElapsed < emitter.duration;
                const float emissionDt = emitter.duration < 0.f
                    ? dt : std::clamp(emitter.duration - previousElapsed, 0.f, dt);

                std::uint32_t requested = 0;
                const std::uint32_t perUpdateLimit = std::max(1u, emitter.maxParticlesPerUpdate);
                const std::uint32_t burst = std::min(runtime.pendingBurstCount, perUpdateLimit);
                runtime.pendingBurstCount -= burst;
                requested += burst;

                if (emitter.emitting && withinDuration && emitter.emissionRate > 0.f && emissionDt > 0.f) {
                    runtime.emissionAccumulator += static_cast<double>(emitter.emissionRate)
                        * static_cast<double>(emissionDt);
                    const double whole = std::floor(runtime.emissionAccumulator);
                    const std::uint32_t budget = perUpdateLimit - requested;
                    const auto available = static_cast<std::uint64_t>(std::min(
                        whole, static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
                    const std::uint32_t continuous = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(available, budget));
                    requested += continuous;
                    // Les pointes d'émission sont bornées : on ne crée pas une dette de particules
                    // qui provoquerait une longue série de frames coûteuses après un ralentissement.
                    runtime.emissionAccumulator -= whole;
                }

                if (requested > 0) {
                    m_batches.push_back({ emitterEntity, transform.getPosition(),
                        transform.getRotation().asRadians(), transform.depth, requested });
                }
            });

        for (const SpawnBatch& batch : m_batches) {
            if (!registry.isValid(batch.emitter)
                || !registry.hasComponent<ParticleEmitter>(batch.emitter)
                || !registry.hasComponent<ParticleEmitterRuntime>(batch.emitter))
                continue;
            ParticleEmitter& emitter = registry.getComponent<ParticleEmitter>(batch.emitter);
            ParticleEmitterRuntime& runtime = registry.getComponent<ParticleEmitterRuntime>(batch.emitter);
            for (std::uint32_t i = 0; i < batch.count; ++i)
                spawnParticle(registry, batch, emitter, runtime);
        }

        m_expiredEntities.clear();
        registry.view<Transform2D, Particle>([&](Entity entity, Transform2D& transform, Particle& particle) {
            if (particle.expired) return;
            particle.age += dt;
            if (particle.age >= particle.lifetime) {
                particle.expired = true;
                m_expiredEntities.push_back(entity);
                return;
            }

            particle.velocity += particle.acceleration * dt;
            transform.move(particle.velocity * dt);
            particle.rotation += particle.angularVelocity * dt;
            transform.setRotation(sf::degrees(particle.rotation));
        });

        // La destruction est différée par Registry jusqu'à flushPendingDestroys().
        for (Entity entity : m_expiredEntities)
            registry.destroyEntity(entity);

        // Recalcule le nombre vivant en un seul passage. Cela reste correct si le jeu
        // supprime un effet manuellement et évite de lier le compteur aux callbacks globaux.
        m_liveCounts.clear();
        registry.view<Particle>([&](Entity, Particle& particle) {
            if (particle.expired || particle.emitter.index == UINT32_MAX) return;
            const std::uint64_t key = (static_cast<std::uint64_t>(particle.emitter.generation) << 32)
                | particle.emitter.index;
            ++m_liveCounts[key];
        });

        m_finishedEmitters.clear();
        registry.view<ParticleEmitter, ParticleEmitterRuntime>(
            [&](Entity entity, ParticleEmitter& emitter, ParticleEmitterRuntime& runtime) {
                const std::uint64_t key = (static_cast<std::uint64_t>(entity.generation) << 32)
                    | entity.index;
                const auto live = m_liveCounts.find(key);
                runtime.liveParticleCount = live == m_liveCounts.end() ? 0 : live->second;
                const bool durationEnded = emitter.duration >= 0.f && runtime.elapsed >= emitter.duration;
                if (emitter.destroyWhenFinished && (!emitter.emitting || durationEnded)
                    && runtime.pendingBurstCount == 0 && runtime.liveParticleCount == 0
                    && !runtime.destroyRequested) {
                    runtime.destroyRequested = true;
                    m_finishedEmitters.push_back(entity);
                }
            });
        for (Entity emitter : m_finishedEmitters)
            registry.destroyEntity(emitter);
    }

private:
    struct SpawnBatch {
        Entity emitter;
        sf::Vector2f position;
        float rotationRadians;
        float depth;
        std::uint32_t count;
    };

    std::vector<SpawnBatch> m_batches;
    std::vector<Entity> m_expiredEntities;
    std::vector<Entity> m_finishedEmitters;
    std::unordered_map<std::uint64_t, std::uint32_t> m_liveCounts;

    static std::uint32_t nextRandom(ParticleEmitterRuntime& runtime) {
        std::uint32_t value = runtime.randomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        runtime.randomState = value == 0 ? 1u : value;
        return runtime.randomState;
    }

    static float randomUnit(ParticleEmitterRuntime& runtime) {
        return static_cast<float>(nextRandom(runtime) >> 8) / 16777215.f;
    }

    static float randomRange(ParticleEmitterRuntime& runtime, float minimum, float maximum) {
        if (maximum < minimum) std::swap(minimum, maximum);
        return minimum + (maximum - minimum) * randomUnit(runtime);
    }

    static sf::Vector2f rotate(sf::Vector2f value, float radians) {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return { value.x * cosine - value.y * sine, value.x * sine + value.y * cosine };
    }

    static void spawnParticle(Registry& registry, const SpawnBatch& batch,
        const ParticleEmitter& emitter, ParticleEmitterRuntime& runtime) {
        const float pi = 3.14159265358979323846f;
        sf::Vector2f localOffset{};
        if (emitter.spawnShape == ParticleEmitter::SpawnShape::Rectangle) {
            localOffset = {
                randomRange(runtime, -std::abs(emitter.spawnArea.x) * 0.5f,
                    std::abs(emitter.spawnArea.x) * 0.5f),
                randomRange(runtime, -std::abs(emitter.spawnArea.y) * 0.5f,
                    std::abs(emitter.spawnArea.y) * 0.5f)
            };
        }
        else if (emitter.spawnShape == ParticleEmitter::SpawnShape::Circle) {
            const float angle = randomRange(runtime, 0.f, 2.f * pi);
            const float radius = std::sqrt(randomUnit(runtime)) * std::max(0.f, emitter.spawnRadius);
            localOffset = { std::cos(angle) * radius, std::sin(angle) * radius };
        }

        const sf::Vector2f origin = batch.position + rotate(localOffset, batch.rotationRadians);
        const float direction = randomRange(runtime, emitter.angleMinDegrees, emitter.angleMaxDegrees)
            * pi / 180.f + batch.rotationRadians;
        const float speed = randomRange(runtime, emitter.speedMin, emitter.speedMax);

        Particle particle;
        particle.emitter = batch.emitter;
        particle.velocity = { std::cos(direction) * speed, std::sin(direction) * speed };
        particle.acceleration = emitter.acceleration;
        particle.lifetime = std::max(0.001f,
            randomRange(runtime, emitter.lifetimeMin, emitter.lifetimeMax));
        particle.angularVelocity = randomRange(runtime,
            emitter.angularVelocityMin, emitter.angularVelocityMax);

        Entity entity = registry.createEntity();
        Transform2D transform;
        transform.setPosition(origin);
        transform.setRotation(sf::degrees(particle.rotation));
        transform.depth = batch.depth;
        registry.addComponent<Transform2D>(entity, transform);
        registry.addComponent<Particle>(entity, std::move(particle));
    }
};
