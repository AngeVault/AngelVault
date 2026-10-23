#pragma once

#include <SFML/System/Vector2.hpp>
#include <cstdint>

// Configuration modifiable d'un émetteur. duration < 0 signifie une émission sans fin.
struct ParticleEmitter {
    enum class SpawnShape { Point, Rectangle, Circle };

    float emissionRate = 0.f; // particules par seconde
    std::uint32_t burstCount = 0; // émission immédiate au démarrage
    float duration = -1.f;
    bool emitting = true;
    bool destroyWhenFinished = false;
    std::uint32_t maxParticlesPerUpdate = 256;

    SpawnShape spawnShape = SpawnShape::Point;
    sf::Vector2f spawnArea{}; // largeur et hauteur du rectangle
    float spawnRadius = 0.f;
    float angleMinDegrees = 0.f;
    float angleMaxDegrees = 360.f;
    float speedMin = 0.f;
    float speedMax = 0.f;
    sf::Vector2f acceleration{};
    float lifetimeMin = 0.5f;
    float lifetimeMax = 1.5f;
    float angularVelocityMin = 0.f;
    float angularVelocityMax = 0.f;
    std::uint32_t randomSeed = 0; // zéro : graine déterministe basée sur l'entité
};

// Etat d'exécution isolé de la configuration pour garder les responsabilités ECS claires.
struct ParticleEmitterRuntime {
    double emissionAccumulator = 0.0;
    std::uint32_t pendingBurstCount = 0;
    std::uint32_t liveParticleCount = 0;
    std::uint32_t randomState = 1;
    float elapsed = 0.f;
    bool destroyRequested = false;
};
