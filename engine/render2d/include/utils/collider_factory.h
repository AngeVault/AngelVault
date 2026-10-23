#pragma once
#include "components/collider2d.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stack>

class ColliderFactory {
public:

    // ---------------------------------------------------------------
    // Génère un Collider2D depuis une texture selon la précision voulue
    // tolerance = pour RDP (plus grand = moins de points = moins précis)
    // alphaThreshold = à partir de quel alpha on considère le pixel "plein"
    // ---------------------------------------------------------------
    static Collider2D fromTexture(const sf::Texture& texture, Collider2D::Precision precision = Collider2D::Precision::Convex, float  tolerance = 2.0f, uint8_t alphaThreshold = 128)
    {
        sf::Image image = texture.copyToImage();
        auto      size = image.getSize();

        // --- Collecte tous les pixels non-transparents ---
        std::vector<sf::Vector2f> solidPixels;
        solidPixels.reserve(size.x * size.y / 4);

        for (uint32_t y = 0; y < size.y; ++y) {
            for (uint32_t x = 0; x < size.x; ++x) {
                if (image.getPixel({ x, y }).a >= alphaThreshold)
                    solidPixels.push_back({ static_cast<float>(x), static_cast<float>(y) });
            }
        }

        if (solidPixels.empty())
            return Collider2D::makeBox({ static_cast<float>(size.x) * 0.5f,
                                        static_cast<float>(size.y) * 0.5f });

        // Centre de la texture (origine du sprite par défaut)
        sf::Vector2f center = { static_cast<float>(size.x) * 0.5f,
                                static_cast<float>(size.y) * 0.5f };

        switch (precision) {
        case Collider2D::Precision::BoundingBox: return buildBoundingBox(solidPixels, center);
        case Collider2D::Precision::Circle:      return buildCircle(solidPixels, center);
        case Collider2D::Precision::Convex:      return buildConvex(solidPixels, center, tolerance);
        case Collider2D::Precision::PixelPerfect:return buildConvex(solidPixels, center, 0.5f); // tolérance min
        }
        return buildBoundingBox(solidPixels, center);
    }

private:

    // ---------------------------------------------------------------
    // BOUNDING BOX : rectangle autour de tous les pixels pleins
    // ---------------------------------------------------------------
    static Collider2D buildBoundingBox(const std::vector<sf::Vector2f>& pts,
        sf::Vector2f center)
    {
        float minX = pts[0].x, maxX = pts[0].x;
        float minY = pts[0].y, maxY = pts[0].y;
        for (auto& p : pts) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        sf::Vector2f halfSize = { (maxX - minX) * 0.5f, (maxY - minY) * 0.5f };
        sf::Vector2f bbCenter = { (minX + maxX) * 0.5f - center.x,
                                  (minY + maxY) * 0.5f - center.y };
        Collider2D c = Collider2D::makeBox(halfSize);
        c.offset = bbCenter;
        return c;
    }

    // ---------------------------------------------------------------
    // CERCLE ENGLOBANT : cercle qui contient tous les pixels pleins
    // ---------------------------------------------------------------
    static Collider2D buildCircle(const std::vector<sf::Vector2f>& pts,
        sf::Vector2f center)
    {
        float maxDistSq = 0.f;
        for (auto& p : pts) {
            float dx = p.x - center.x;
            float dy = p.y - center.y;
            float distSq = dx * dx + dy * dy;
            maxDistSq = std::max(maxDistSq, distSq);
        }
        return Collider2D::makeCircle(std::sqrt(maxDistSq));
    }

    // ---------------------------------------------------------------
    // CONVEX HULL + RDP
    // 1. Calcule l'enveloppe convexe (Graham scan)
    // 2. Réduit les points avec Ramer-Douglas-Peucker
    // ---------------------------------------------------------------
    static Collider2D buildConvex(const std::vector<sf::Vector2f>& pts,
        sf::Vector2f center, float tolerance)
    {
        auto hull = grahamScan(pts);

        if (tolerance > 0.5f)
            hull = rdp(hull, tolerance);

        // Convertit en coordonnées locales (relatives au centre)
        for (auto& p : hull) {
            p.x -= center.x;
            p.y -= center.y;
        }

        if (hull.size() < 3)
            return buildBoundingBox(pts, center);

        return Collider2D::makePolygon(hull);
    }

    // ---------------------------------------------------------------
    // GRAHAM SCAN — calcule l'enveloppe convexe en O(n log n)
    // Retourne les points dans l'ordre anti-horaire
    // ---------------------------------------------------------------
    static std::vector<sf::Vector2f> grahamScan(std::vector<sf::Vector2f> pts) {
        // Trouver le point le plus bas (puis le plus à gauche en cas d'égalité)
        size_t pivot = 0;
        for (size_t i = 1; i < pts.size(); ++i) {
            if (pts[i].y < pts[pivot].y ||
                (pts[i].y == pts[pivot].y && pts[i].x < pts[pivot].x))
                pivot = i;
        }
        std::swap(pts[0], pts[pivot]);
        sf::Vector2f p0 = pts[0];

        // Tri par angle polaire par rapport à p0
        std::sort(pts.begin() + 1, pts.end(), [&](const sf::Vector2f& a, const sf::Vector2f& b) {
            float cross = (a.x - p0.x) * (b.y - p0.y) - (a.y - p0.y) * (b.x - p0.x);
            if (std::abs(cross) > 1e-6f) return cross > 0.f; // anti-horaire
            float da = (a.x - p0.x) * (a.x - p0.x) + (a.y - p0.y) * (a.y - p0.y);
            float db = (b.x - p0.x) * (b.x - p0.x) + (b.y - p0.y) * (b.y - p0.y);
            return da < db; // plus proche en premier si même angle
            });

        // Construction de l'enveloppe
        std::vector<sf::Vector2f> hull;
        for (auto& p : pts) {
            while (hull.size() >= 2) {
                sf::Vector2f a = hull[hull.size() - 2];
                sf::Vector2f b = hull[hull.size() - 1];
                // Produit vectoriel (b-a) × (p-a) — si <= 0 : virage droite = on retire
                float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
                if (cross <= 0.f) hull.pop_back();
                else break;
            }
            hull.push_back(p);
        }
        return hull;
    }

    // ---------------------------------------------------------------
    // RAMER-DOUGLAS-PEUCKER — simplifie un polygone en O(n log n)
    // tolerance = distance max autorisée entre un point et la droite simplifiée
    // Plus la tolérance est grande, moins il y a de points → forme plus grossière
    // ---------------------------------------------------------------
    static std::vector<sf::Vector2f> rdp(const std::vector<sf::Vector2f>& pts,
        float tolerance)
    {
        if (pts.size() <= 2) return pts;

        // Trouve le point le plus éloigné du segment [premier, dernier]
        float maxDist = 0.f;
        size_t index = 0;
        for (size_t i = 1; i < pts.size() - 1; ++i) {
            float d = pointToSegmentDist(pts[i], pts.front(), pts.back());
            if (d > maxDist) { maxDist = d; index = i; }
        }

        if (maxDist > tolerance) {
            // Récursion sur les deux sous-listes
            auto left = rdp({ pts.begin(), pts.begin() + index + 1 }, tolerance);
            auto right = rdp({ pts.begin() + index, pts.end() }, tolerance);
            left.insert(left.end(), right.begin() + 1, right.end());
            return left;
        }
        // Tous les points intermédiaires sont sous la tolérance → on les supprime
        return { pts.front(), pts.back() };
    }

    // Distance d'un point P au segment [A, B]
    static float pointToSegmentDist(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
        float dx = b.x - a.x, dy = b.y - a.y;
        float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-6f) return std::hypot(p.x - a.x, p.y - a.y);
        float t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq, 0.f, 1.f);
        return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
    }
};