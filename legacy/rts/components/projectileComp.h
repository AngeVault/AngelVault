#pragma once
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include "unitComp.h"

struct Projectile {
    sf::Vector2f dir{ 1.f, 0.f }; 
    float speed = 600.f;          
    int damage = 10;
    float life = 2.5f;          
    ECS::Unit::Team ownerTeam = ECS::Unit::Team::Player;
    bool hasHit = false;
};
