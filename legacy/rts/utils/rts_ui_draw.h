#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace RtsUI {
    inline void drawRoundedRect(sf::RenderTarget& target, sf::FloatRect bounds, sf::Color fill, sf::Color outline = sf::Color::Transparent, float outlineThickness = 0.f, float radius = 10.f) {
        constexpr int cornerSegments = 6;
        constexpr float pi = 3.14159265358979323846f;
        const float r = std::clamp(radius, 0.f,
            std::min(bounds.size.x, bounds.size.y) * 0.5f);
        const std::array<sf::Vector2f, 4> centers = {{
            { bounds.position.x + r, bounds.position.y + r },
            { bounds.position.x + bounds.size.x - r, bounds.position.y + r },
            { bounds.position.x + bounds.size.x - r, bounds.position.y + bounds.size.y - r },
            { bounds.position.x + r, bounds.position.y + bounds.size.y - r }
        }};
        const std::array<float, 4> starts = { 180.f, 270.f, 0.f, 90.f };
        sf::ConvexShape shape;
        shape.setPointCount(cornerSegments * 4 + 4);
        size_t point = 0;
        for (size_t corner = 0; corner < centers.size(); ++corner) {
            for (int segment = 0; segment <= cornerSegments; ++segment) {
                const float angle = (starts[corner] + 90.f * segment / cornerSegments) * pi / 180.f;
                shape.setPoint(point++, centers[corner] + sf::Vector2f(
                    std::cos(angle) * r, std::sin(angle) * r));
            }
        }
        shape.setFillColor(fill);
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(outlineThickness);
        target.draw(shape);
    }
}
