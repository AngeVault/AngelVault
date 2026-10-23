#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Color.hpp>
#include <string>

namespace ECS {
    struct BuildingVisual {
        std::string textureId;
        sf::IntRect textureRect{ { 0, 0 }, { 64, 64 } };
        sf::Color tint = sf::Color::White;
    };
}
