#pragma once
#include <SFML/Graphics.hpp>

struct SpriteComponent {
    sf::Sprite sprite;
    int        layer = 0;

    SpriteComponent() = default;

    explicit SpriteComponent(const sf::Texture& texture, int layer = 0)
        : sprite(texture), layer(layer) {
    }
};