#pragma once
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <string>

class TextureFactory {
public:

    // Crée une texture de couleur unie
    static sf::Texture createSolid(sf::Vector2u size, sf::Color color) {
        sf::Image image(size, color);
        sf::Texture texture;
        texture.loadFromImage(image);
        return texture;
    }

    // Crée une texture avec un contour
    static sf::Texture createOutlined(sf::Vector2u size, sf::Color fill, sf::Color outline, uint32_t thickness = 2) {
        sf::Image image(size, fill);

        // Dessine le contour pixel par pixel
        for (uint32_t x = 0; x < size.x; ++x) {
            for (uint32_t y = 0; y < size.y; ++y) {
                bool isBorder = x < thickness || x >= size.x - thickness
                    || y < thickness || y >= size.y - thickness;
                if (isBorder)
                    image.setPixel({ x, y }, outline);
            }
        }

        sf::Texture texture;
        texture.loadFromImage(image);
        return texture;
    }

    // Crée une texture en damier (utile pour débugger les UV)
    static sf::Texture createCheckerboard(sf::Vector2u size, sf::Color colorA, sf::Color colorB, uint32_t cellSize = 8) 
    {
        sf::Image image(size);

        for (uint32_t x = 0; x < size.x; ++x) 
        {
            for (uint32_t y = 0; y < size.y; ++y) 
            {
                bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
                image.setPixel({ x, y }, isEven ? colorA : colorB);
            }
        }

        sf::Texture texture;
        texture.loadFromImage(image);
        return texture;
    }
};