#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <variant>


struct GeometryShapeStyle {
    sf::Color fillColor = sf::Color::White;
    sf::Color outlineColor = sf::Color::Transparent;
    float outlineThickness = 0.f;
    std::string textureId = "";
};

struct Geometry {
    using ShapeStyle = GeometryShapeStyle;   // même nom d'usage : Geometry::ShapeStyle

    struct BoxData { sf::Vector2f halfSize; ShapeStyle style; };
    struct CircleData { float radius; ShapeStyle style; };
    struct PolygonData { std::vector<sf::Vector2f> points; ShapeStyle style; };

    struct SpriteData {
        std::string textureId;
        sf::IntRect rect{};
        sf::Color tint = sf::Color::White;
    }; 

    using GeoData = std::variant<BoxData, CircleData, PolygonData, SpriteData>;
    GeoData data;
	int layer = 0;
}; 