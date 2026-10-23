#pragma once

#include <SFML/System/Vector2.hpp>
#include <ecs/entity.h>

// Une particule est une entite ECS classique : Transform2D + Particle.
// Les textures sont partagees et conservees en vie tant qu'une particule les utilise.
struct Particle {
    Entity emitter = NULL_ENTITY;
    sf::Vector2f velocity{};
    sf::Vector2f acceleration{};
    float age = 0.f;
    float lifetime = 1.f;
    float rotation = 0.f; // degrés
    float angularVelocity = 0.f; // degrés par seconde
    bool expired = false;
};
