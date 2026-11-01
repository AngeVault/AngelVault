#pragma once
// Formes des entités dans l'éditeur : boîtes englobantes, contours des collisions et redimensionnement
// (outil Taille de la vue scène). Coordonnées « locales » :
//  - Geometry   : repère de l'entité (Transform2D complet : position, rotation, échelle) ;
//  - Collider2D : repère position + rotation (comme RenderCollisionSystem / la physique), sans échelle ;
//                 la boîte inclut le décalage (offset) du collider.
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/transform2d.h"

#include <SFML/Graphics/Transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace shape_edit {

struct LocalBox {
    sf::Vector2f center;   // centre de la boîte (repère local)
    sf::Vector2f half;     // demi-taille
};

inline LocalBox boundsOf(const std::vector<sf::Vector2f>& pts, sf::Vector2f offset = {}) {
    if (pts.empty()) return { offset, { 8.f, 8.f } };
    sf::Vector2f mn = pts[0], mx = pts[0];
    for (sf::Vector2f p : pts) { mn = { std::min(mn.x, p.x), std::min(mn.y, p.y) }; mx = { std::max(mx.x, p.x), std::max(mx.y, p.y) }; }
    return { offset + (mn + mx) * 0.5f, (mx - mn) * 0.5f };
}

// ---- Geometry ------------------------------------------------------------------------------------

inline LocalBox geometryBox(const Geometry& g) {
    return std::visit([](const auto& d) -> LocalBox {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, Geometry::BoxData>) return { {}, d.halfSize };
        else if constexpr (std::is_same_v<T, Geometry::CircleData>) return { {}, { d.radius, d.radius } };
        else if constexpr (std::is_same_v<T, Geometry::SpriteData>) return { {}, sf::Vector2f(d.rect.size) * 0.5f };
        else return boundsOf(d.points);
    }, g.data);
}

// ---- Collider2D ----------------------------------------------------------------------------------

// Repère du collider dans le monde (la rotation ne s'applique qu'au rectangle, comme RenderCollisionSystem)
inline sf::Transform colliderToWorld(const Transform2D& t, const Collider2D& c) {
    sf::Transform m;
    m.translate(t.getPosition());
    if (c.type() == Collider2D::ShapeType::Box) {
        m.translate(c.offset);
        m.rotate(t.getRotation());
        m.translate(-c.offset);
    }
    return m;
}

inline LocalBox colliderBox(const Collider2D& c) {
    using C = Collider2D;
    return std::visit([&](const auto& s) -> LocalBox {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, C::Box>) return { c.offset, s.halfSize };
        else if constexpr (std::is_same_v<T, C::Circle>) return { c.offset, { s.radius, s.radius } };
        else if constexpr (std::is_same_v<T, C::Ellipse>) return { c.offset, s.radii };
        else if constexpr (std::is_same_v<T, C::Capsule>) return { c.offset, { s.radius, s.height * 0.5f } };
        else return boundsOf(s.points, c.offset);
    }, c.shape);
}

// Contour du collider (repère local du collider, décalage compris)
inline std::vector<sf::Vector2f> colliderOutline(const Collider2D& c) {
    using C = Collider2D;
    std::vector<sf::Vector2f> pts;
    constexpr float kPi = 3.14159265f;
    auto ellipse = [&](sf::Vector2f r) {
        for (int i = 0; i < 48; ++i) {
            const float a = 2.f * kPi * float(i) / 48.f;
            pts.push_back(c.offset + sf::Vector2f{ std::cos(a) * r.x, std::sin(a) * r.y });
        }
    };
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, C::Box>) {
            const sf::Vector2f h = s.halfSize;
            pts = { c.offset + sf::Vector2f{ -h.x, -h.y }, c.offset + sf::Vector2f{ h.x, -h.y },
                    c.offset + sf::Vector2f{ h.x, h.y },   c.offset + sf::Vector2f{ -h.x, h.y } };
        } else if constexpr (std::is_same_v<T, C::Circle>) {
            ellipse({ s.radius, s.radius });
        } else if constexpr (std::is_same_v<T, C::Ellipse>) {
            ellipse(s.radii);
        } else if constexpr (std::is_same_v<T, C::Capsule>) {
            // verticale : deux demi-cercles reliés (hauteur totale = height)
            const float body = std::max(0.f, s.height * 0.5f - s.radius);
            for (int i = 0; i <= 16; ++i) {   // haut
                const float a = kPi + kPi * float(i) / 16.f;
                pts.push_back(c.offset + sf::Vector2f{ std::cos(a) * s.radius, -body + std::sin(a) * s.radius });
            }
            for (int i = 0; i <= 16; ++i) {   // bas
                const float a = kPi * float(i) / 16.f;
                pts.push_back(c.offset + sf::Vector2f{ std::cos(a) * s.radius, body + std::sin(a) * s.radius });
            }
        } else {
            for (sf::Vector2f p : s.points) pts.push_back(c.offset + p);
        }
    }, c.shape);
    return pts;
}

// ---- Redimensionnement ---------------------------------------------------------------------------
// s : facteurs d'échelle (> 0) ; anchor : point fixe, dans le repère local de la forme ;
// dir : poignée tenue (-1, 0 ou 1 par axe) — sert aux formes à une seule dimension (cercle).

inline float uniformFactor(sf::Vector2f s, sf::Vector2i dir) {
    if (dir.x != 0 && dir.y != 0) return std::max(s.x, s.y);
    return dir.x != 0 ? s.x : s.y;
}

inline sf::Vector2f scaleAbout(sf::Vector2f p, sf::Vector2f anchor, sf::Vector2f s) {
    return { anchor.x + (p.x - anchor.x) * s.x, anchor.y + (p.y - anchor.y) * s.y };
}

// `start` : la Geometry au début du glisser. Un sprite change l'échelle de son Transform2D.
inline void scaleGeometry(Geometry& g, const Geometry& start, Transform2D& t, sf::Vector2f startScale,
                          sf::Vector2f s, sf::Vector2f anchor, sf::Vector2i dir) {
    std::visit([&](auto& d) {
        using T = std::decay_t<decltype(d)>;
        const T& d0 = std::get<T>(start.data);
        if constexpr (std::is_same_v<T, Geometry::BoxData>) d.halfSize = { d0.halfSize.x * s.x, d0.halfSize.y * s.y };
        else if constexpr (std::is_same_v<T, Geometry::CircleData>) d.radius = d0.radius * uniformFactor(s, dir);
        else if constexpr (std::is_same_v<T, Geometry::SpriteData>) t.setScale({ startScale.x * s.x, startScale.y * s.y });
        else for (std::size_t i = 0; i < d.points.size() && i < d0.points.size(); ++i) d.points[i] = scaleAbout(d0.points[i], anchor, s);
    }, g.data);
}

inline void scaleCollider(Collider2D& c, const Collider2D& start, sf::Vector2f s, sf::Vector2f anchor, sf::Vector2i dir) {
    using C = Collider2D;
    const LocalBox b0 = colliderBox(start);
    std::visit([&](auto& d) {
        using T = std::decay_t<decltype(d)>;
        const T& d0 = std::get<T>(start.shape);
        if constexpr (std::is_same_v<T, C::Box>) d.halfSize = { d0.halfSize.x * s.x, d0.halfSize.y * s.y };
        else if constexpr (std::is_same_v<T, C::Circle>) d.radius = d0.radius * uniformFactor(s, dir);
        else if constexpr (std::is_same_v<T, C::Ellipse>) d.radii = { d0.radii.x * s.x, d0.radii.y * s.y };
        else if constexpr (std::is_same_v<T, C::Capsule>) { d.radius = d0.radius * s.x; d.height = d0.height * s.y; }
        else for (std::size_t i = 0; i < d.points.size() && i < d0.points.size(); ++i)   // points relatifs au décalage
            d.points[i] = scaleAbout(start.offset + d0.points[i], anchor, s) - start.offset;
    }, c.shape);
    if (c.type() != C::ShapeType::Polygon) {   // le centre suit le point fixe (bord opposé immobile)
        sf::Vector2f k = s;
        if (c.type() == C::ShapeType::Circle) k = { uniformFactor(s, dir), uniformFactor(s, dir) };
        c.offset = scaleAbout(b0.center, anchor, k);
    }
}

} // namespace shape_edit
