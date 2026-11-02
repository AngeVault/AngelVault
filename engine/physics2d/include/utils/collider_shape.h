#pragma once
// Forme d'un Collider2D dans le monde — UNE seule définition, utilisée par la physique (CollisionSystem),
// le dessin de debug (RenderCollisionSystem) et l'éditeur. Ce que l'on voit = ce qui collisionne.
//
// Repère : la forme (décalage compris) tourne avec l'entité autour de sa position (Transform2D),
// puis est déplacée à cette position. L'échelle du Transform2D n'est PAS appliquée aux collisions.
//
// Les formes courbes (ellipse, capsule) sont approchées par des polygones convexes ; le cercle reste exact
// dans les tests (CollisionSystem), son contour ne sert qu'au dessin.
#include "components/collider2d.h"
#include "components/transform2d.h"

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace collider_shape {

inline constexpr int kCurveSegments = 24;   // points pour un tour complet (ellipse, cercle dessiné)

// Point local (repère de l'entité, décalage compris) -> monde
inline sf::Vector2f toWorld(const Transform2D& t, sf::Vector2f local) {
    const float a = t.getRotation().asRadians();
    const float c = std::cos(a), s = std::sin(a);
    return { t.getPosition().x + local.x * c - local.y * s, t.getPosition().y + local.x * s + local.y * c };
}

// Centre de la forme dans le monde (le décalage tourne avec l'entité)
inline sf::Vector2f worldCenter(const Collider2D& c, const Transform2D& t) { return toWorld(t, c.offset); }

// Contour dans le repère de l'entité (décalage compris), sens horaire à l'écran
inline std::vector<sf::Vector2f> localOutline(const Collider2D& col, int segments = kCurveSegments) {
    using C = Collider2D;
    constexpr float kPi = 3.14159265358979f;
    segments = std::max(8, segments);
    std::vector<sf::Vector2f> pts;
    const sf::Vector2f o = col.offset;
    auto ellipse = [&](sf::Vector2f r) {
        pts.reserve(std::size_t(segments));
        for (int i = 0; i < segments; ++i) {
            const float a = 2.f * kPi * float(i) / float(segments);
            pts.push_back(o + sf::Vector2f{ std::cos(a) * r.x, std::sin(a) * r.y });
        }
    };
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, C::Box>) {
            const sf::Vector2f h = s.halfSize;
            pts = { o + sf::Vector2f{ -h.x, -h.y }, o + sf::Vector2f{ h.x, -h.y },
                    o + sf::Vector2f{ h.x, h.y },   o + sf::Vector2f{ -h.x, h.y } };
        } else if constexpr (std::is_same_v<T, C::Circle>) {
            ellipse({ s.radius, s.radius });
        } else if constexpr (std::is_same_v<T, C::Ellipse>) {
            ellipse(s.radii);
        } else if constexpr (std::is_same_v<T, C::Capsule>) {
            // Verticale : hauteur totale = height (demi-cercles compris), largeur = 2 * radius
            const float r = std::max(0.f, s.radius);
            const float body = std::max(0.f, s.height * 0.5f - r);   // demi-longueur de la partie droite
            const int half = std::max(4, segments / 2);
            for (int i = 0; i <= half; ++i) {   // demi-cercle du haut (de gauche à droite)
                const float a = kPi + kPi * float(i) / float(half);
                pts.push_back(o + sf::Vector2f{ std::cos(a) * r, -body + std::sin(a) * r });
            }
            for (int i = 0; i <= half; ++i) {   // demi-cercle du bas (de droite à gauche)
                const float a = kPi * float(i) / float(half);
                pts.push_back(o + sf::Vector2f{ std::cos(a) * r, body + std::sin(a) * r });
            }
        } else {
            pts.reserve(s.points.size());
            for (sf::Vector2f p : s.points) pts.push_back(o + p);
        }
    }, col.shape);
    return pts;
}

// Contour dans le monde
inline std::vector<sf::Vector2f> worldOutline(const Collider2D& col, const Transform2D& t, int segments = kCurveSegments) {
    std::vector<sf::Vector2f> pts = localOutline(col, segments);
    for (sf::Vector2f& p : pts) p = toWorld(t, p);
    return pts;
}

// Rayon englobant autour du centre de la forme (sans le décalage)
inline float boundingRadius(const Collider2D& col) {
    using C = Collider2D;
    return std::visit([&](const auto& s) -> float {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, C::Box>) return std::hypot(s.halfSize.x, s.halfSize.y);
        else if constexpr (std::is_same_v<T, C::Circle>) return s.radius;
        else if constexpr (std::is_same_v<T, C::Ellipse>) return std::max(s.radii.x, s.radii.y);
        else if constexpr (std::is_same_v<T, C::Capsule>) return std::max(s.radius, s.height * 0.5f);
        else {
            float r = 0.f;
            for (sf::Vector2f p : s.points) r = std::max(r, std::hypot(p.x, p.y));
            return r;
        }
    }, col.shape);
}

// Angle « droit » (0° ou 180°) : un rectangle reste aligné sur les axes
inline bool isAxisAligned(const Transform2D& t) {
    return std::abs(std::remainder(t.getRotation().asDegrees(), 180.f)) < 0.01f;
}

} // namespace collider_shape
