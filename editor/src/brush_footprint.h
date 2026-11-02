#pragma once
// Cases couvertes par le pinceau (Pinceau / Gomme) autour de la case visée, et tracé de lignes de cases.
#include "editor_context.h"

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

// Décalages (en cases) couverts par un pinceau de côté `size`. La case visée est au centre
// (taille paire : le centre est entre deux cases, la case visée est celle en bas à droite du centre).
inline std::vector<sf::Vector2i> brushFootprint(int size, BrushShape shape) {
    size = std::max(1, size);
    std::vector<sf::Vector2i> out;
    out.reserve(std::size_t(size) * size);
    const int start = -(size - 1) / 2;
    const float r = size * 0.5f;
    for (int dy = 0; dy < size; ++dy)
        for (int dx = 0; dx < size; ++dx) {
            if (shape == BrushShape::Circle && size > 2) {
                const float fx = dx + 0.5f - r, fy = dy + 0.5f - r;
                if (fx * fx + fy * fy > r * r + 0.25f) continue;
            }
            out.push_back({ start + dx, start + dy });
        }
    return out;
}

// Cases d'une ligne de `a` à `b` (Bresenham), extrémités comprises
inline std::vector<sf::Vector2i> cellLine(sf::Vector2i a, sf::Vector2i b) {
    std::vector<sf::Vector2i> out;
    int dx = std::abs(b.x - a.x), dy = -std::abs(b.y - a.y);
    const int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        out.push_back(a);
        if (a == b) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; a.x += sx; }
        if (e2 <= dx) { err += dx; a.y += sy; }
    }
    return out;
}

// Division entière arrondie vers le bas (aussi pour les négatifs)
inline int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
