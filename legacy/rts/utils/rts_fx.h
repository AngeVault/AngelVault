#pragma once

#include "../components/rts_particle_visual.h"
#include <components/particle_emitter.h>
#include <ecs/registry.h>
#include <utils/particle_manager.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <cstdint>

// petits outils pour lancer des rafales de particules
namespace Rts::Fx {

    inline RtsParticleVisual visual(sf::Color from, sf::Color to, float startRadius, float endRadius,
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

    inline ParticleEmitter emitter(float speedMin, float speedMax, float lifeMin, float lifeMax,
        sf::Vector2f acceleration) {
        ParticleEmitter e;
        e.speedMin = speedMin;
        e.speedMax = speedMax;
        e.lifetimeMin = lifeMin;
        e.lifetimeMax = lifeMax;
        e.acceleration = acceleration;
        return e;
    }

    // les particules apparaissent n'importe ou dans un rectangle centre sur l'emetteur
    inline ParticleEmitter inRect(ParticleEmitter e, sf::Vector2f size) {
        e.spawnShape = ParticleEmitter::SpawnShape::Rectangle;
        e.spawnArea = size;
        return e;
    }

    inline ParticleEmitter upward(ParticleEmitter e, float spread) {
        e.angleMinDegrees = -90.f - spread;
        e.angleMaxDegrees = -90.f + spread;
        return e;
    }

    inline void burst(Registry& registry, sf::Vector2f position, const ParticleEmitter& config,
        const RtsParticleVisual& look, int count) {
        if (count <= 0) return;
        const Entity entity = ParticleManager::emitBurst(registry, position, config,
            static_cast<std::uint32_t>(count));
        if (registry.isValid(entity))
            registry.addComponent<RtsParticleVisual>(entity, look);
    }

}
