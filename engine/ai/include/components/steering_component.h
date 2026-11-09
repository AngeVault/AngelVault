//
// Created by p.pagliazzo on 09/11/2026.
//

#pragma once
#include <string>
#include <vector>
#include <SFML/System/Vector2.hpp>

struct Priority
{
    int priority;
    std::string name;
};

struct SteeringComponent
{
    sf::Vector2f direction; // entre -1 et 1 le total doit faire 1 ou 0;
    float rapidite;
};