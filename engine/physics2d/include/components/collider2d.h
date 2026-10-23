#pragma once
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <variant>

struct Collider2D {

    // types de formes
    enum class ShapeType { Box, Circle, Ellipse, Capsule, Polygon };

    // Precision pour les colliders générés depuis une texture 
    enum class Precision {
        BoundingBox,   // AABB — 1 rectangle, le plus rapide
        Circle,        // Cercle englobant — 1 rayon
        Convex,        // Enveloppe convexe simplifiée — bon compromis
        PixelPerfect   // Contour exact — le plus précis, le plus lent
    };

    // Donnees des formes 
    struct Box { sf::Vector2f halfSize = { 16.f, 16.f }; };
    struct Circle { float radius = 16.f; };
    struct Ellipse { sf::Vector2f radii = { 16.f, 8.f }; };
    struct Capsule { float radius = 8.f; float height = 24.f; };
    struct Polygon { std::vector<sf::Vector2f> points; }; // points locaux, relatifs au centre

    using ShapeData = std::variant<Box, Circle, Ellipse, Capsule, Polygon>;

    // Données du composant
    ShapeData     shape = Box{}; // forme par défaut : rectangle
    sf::Vector2f  offset = { 0.f, 0.f }; // décalage par rapport à Transform2D
    bool          isTrigger = false; // true = détecte sans résoudre la physique
    float         restitution = 0.2f; // rebond [0..1]
    float         friction = 0.4f; // frottement [0..1]

    // Helpers de création rapide
    static Collider2D makeBox(sf::Vector2f halfSize, bool trigger = false) {
        Collider2D c;
        c.shape = Box{ halfSize };
        c.isTrigger = trigger;
        return c;
    }

    static Collider2D makeCircle(float radius, bool trigger = false) {
        Collider2D c;
        c.shape = Circle{ radius };
        c.isTrigger = trigger;
        return c;
    }

    static Collider2D makeEllipse(sf::Vector2f radii, bool trigger = false) {
        Collider2D c;
        c.shape = Ellipse{ radii };
        c.isTrigger = trigger;
        return c;
    }

    static Collider2D makeCapsule(float radius, float height, bool trigger = false) {
        Collider2D c;
        c.shape = Capsule{ radius, height };
        c.isTrigger = trigger;
        return c;
    }

    static Collider2D makePolygon(std::vector<sf::Vector2f> points, bool trigger = false) {
        Collider2D c;
        c.shape = Polygon{ std::move(points) };
        c.isTrigger = trigger;
        return c;
    }

    ShapeType type() const {
        return std::visit([](auto& s) -> ShapeType {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Box>)     return ShapeType::Box;
            if constexpr (std::is_same_v<T, Circle>)  return ShapeType::Circle;
            if constexpr (std::is_same_v<T, Ellipse>) return ShapeType::Ellipse;
            if constexpr (std::is_same_v<T, Capsule>) return ShapeType::Capsule;
            return ShapeType::Polygon;
            }, shape);
    }
};