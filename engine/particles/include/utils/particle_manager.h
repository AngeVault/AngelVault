#pragma once

#include <components/particle_emitter.h>
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <algorithm>
#include <cstdint>

// Façade pratique : les effets restent des composants et des entités du Registry.
// Cette classe ne possède aucun état global et peut être utilisée avec plusieurs Registry.
class ParticleManager {
public:
    // Ajoute un émetteur à une entité existante (pratique pour suivre un personnage).
    static bool attachEmitter(Registry& registry, Entity entity, const ParticleEmitter& config) {
        if (!registry.isValid(entity) || registry.hasComponent<ParticleEmitter>(entity)
            || registry.hasComponent<ParticleEmitterRuntime>(entity))
            return false;
        if (!registry.hasComponent<Transform2D>(entity))
            registry.addComponent<Transform2D>(entity, Transform2D{});
        registry.addComponent<ParticleEmitter>(entity, config);
        ParticleEmitterRuntime runtime;
        runtime.pendingBurstCount = config.burstCount;
        runtime.randomState = config.randomSeed != 0 ? config.randomSeed
            : (entity.index * 747796405u + entity.generation * 2891336453u + 277803737u);
        if (runtime.randomState == 0) runtime.randomState = 1;
        registry.addComponent<ParticleEmitterRuntime>(entity, runtime);
        return true;
    }

    // Crée une entité autonome composée de Transform2D + émetteur + état runtime.
    static Entity createEmitter(Registry& registry, sf::Vector2f position,
        const ParticleEmitter& config) {
        const Entity entity = registry.createEntity();
        Transform2D transform;
        transform.setPosition(position);
        registry.addComponent<Transform2D>(entity, transform);
        attachEmitter(registry, entity, config);
        return entity;
    }

    // Crée un effet ponctuel. L'émetteur est supprimé quand toutes ses particules sont mortes.
    static Entity emitBurst(Registry& registry, sf::Vector2f position,
        ParticleEmitter config, std::uint32_t count) {
        config.emissionRate = 0.f;
        config.burstCount = count;
        config.emitting = false;
        config.duration = 0.f;
        config.destroyWhenFinished = true;
        return createEmitter(registry, position, config);
    }

    // Ajoute une émission ponctuelle à un émetteur déjà actif.
    static bool queueBurst(Registry& registry, Entity emitter, std::uint32_t count) {
        if (!registry.isValid(emitter)
            || !registry.hasComponent<ParticleEmitterRuntime>(emitter))
            return false;
        auto& runtime = registry.getComponent<ParticleEmitterRuntime>(emitter);
        if (runtime.destroyRequested) return false;
        runtime.pendingBurstCount = count > UINT32_MAX - runtime.pendingBurstCount
            ? UINT32_MAX : runtime.pendingBurstCount + count;
        return true;
    }

    static bool setEmitting(Registry& registry, Entity emitter, bool enabled) {
        if (!registry.isValid(emitter) || !registry.hasComponent<ParticleEmitter>(emitter))
            return false;
        if (registry.hasComponent<ParticleEmitterRuntime>(emitter)
            && registry.getComponent<ParticleEmitterRuntime>(emitter).destroyRequested)
            return false;
        registry.getComponent<ParticleEmitter>(emitter).emitting = enabled;
        return true;
    }

    // Demande la destruction de l'émetteur après la fin des particules déjà émises.
    static bool stop(Registry& registry, Entity emitter, bool destroyWhenFinished = true) {
        if (!registry.isValid(emitter) || !registry.hasComponent<ParticleEmitter>(emitter))
            return false;
        if (registry.hasComponent<ParticleEmitterRuntime>(emitter)
            && registry.getComponent<ParticleEmitterRuntime>(emitter).destroyRequested)
            return false;
        ParticleEmitter& config = registry.getComponent<ParticleEmitter>(emitter);
        config.emitting = false;
        config.destroyWhenFinished = destroyWhenFinished;
        return true;
    }
};
