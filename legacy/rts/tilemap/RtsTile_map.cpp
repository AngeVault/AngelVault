#include "RtsTile_map.hpp"
#include <algorithm>

namespace {
    constexpr int GROW_STEP = 16;
    inline int roundUp(int v, int step) { return (v + step - 1) / step * step; }
}

void TileMap::setLayerOrder(std::initializer_list<TileType> order) {
    layerOrder.assign(order.begin(), order.end());
    layerRank.fill(0);
    uint8_t r = 1;
    for (TileType t : layerOrder) layerRank[(uint8_t)t] = r++;
    ++layerVersion; ++version;
}

void TileMap::create(uint32_t w, uint32_t h) {
    width = w;
    height = h;
    nbTiles = w * h;

    value.assign((size_t)(w + 1) * (h + 1), TF_SOLID); 
    walls.assign(nbTiles, 0);

    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            value[vIdx(x, y)] = (uint8_t)TileType::Grass;

    pendingShift = { 0, 0 };
    ++version;
}

void TileMap::writeTile(int x, int y, TileType type) {
    uint8_t solid = (type == TileType::None ||
        type == TileType::Water ||
        type == TileType::Wall) ? TF_SOLID : 0;
    value[vIdx(x, y)] = (uint8_t)type | solid;
}

void TileMap::setTile(int x, int y, TileType type) {
    ensureInBounds(x, y);
    writeTile(x, y, type);
    ++version;     
}

void TileMap::fillRect(int x, int y, int w, int h, TileType type) {
    if (w <= 0 || h <= 0) return;
    int x1 = x + w - 1, y1 = y + h - 1;

    sf::Vector2i shift;
    ensureInBounds(x, y, &shift);
    x1 += shift.x; y1 += shift.y;
    ensureInBounds(x1, y1);

    for (int yy = y; yy <= y1; ++yy)
        for (int xx = x; xx <= x1; ++xx)
            writeTile(xx, yy, type);
    ++version;
}

bool TileMap::update(float dt) {
    auto step = [dt](TileAnimation& anim) {
        if (anim.frameCount <= 1 || anim.frameTime <= 0.f) return false;
        anim.time += dt;
        if (anim.time < anim.frameTime) return false;
        int steps = (int)(anim.time / anim.frameTime);
        anim.time -= steps * anim.frameTime;
        anim.currentFrame = (anim.currentFrame + steps) % anim.frameCount;
        return true;
        };
    bool changed = false;
    for (auto& [type, anim] : tileset.animations)        changed |= step(anim);
    for (auto& [id, anim] : tileset.textureAnimations) changed |= step(anim);
    if (changed) ++animVersion;
    return changed;
}

int TileMap::vertexPieces(int vx, int vy, VertexPiece out[4]) const {
    if (width == 0 || height == 0) return 0;

    static const int     dx[4] = { -1, 0, -1, 0 };
    static const int     dy[4] = { -1, -1, 0, 0 };
    static const uint8_t bit[4] = { 8, 4, 2, 1 };

    TileType t[4];
    uint8_t  r[4];
    for (int i = 0; i < 4; ++i) {
        // hors carte = on prolonge la tile du bord (pas de liseré sur le bord de la carte)
        int x = std::clamp(vx + dx[i], 0, (int)width - 1);
        int y = std::clamp(vy + dy[i], 0, (int)height - 1);
        t[i] = (TileType)(value[vIdx(x, y)] & TF_TYPE_VALUE);
        r[i] = layerRank[(uint8_t)t[i]];
    }

    // rangs distincts (>0), triés croissants
    uint8_t ranks[4]; TileType types[4]; int n = 0;
    for (int i = 0; i < 4; ++i) {
        if (r[i] == 0) continue;
        bool dup = false;
        for (int k = 0; k < n; ++k) if (ranks[k] == r[i]) dup = true;
        if (dup) continue;
        int k = n++;
        while (k > 0 && ranks[k - 1] > r[i]) { ranks[k] = ranks[k - 1]; types[k] = types[k - 1]; --k; }
        ranks[k] = r[i]; types[k] = t[i];
    }

    for (int k = 0; k < n; ++k) {
        uint8_t mask = 0;
        for (int i = 0; i < 4; ++i) if (r[i] >= ranks[k]) mask |= bit[i];
        out[k] = { types[k], mask };
    }
    return n;
}

void TileMap::ensureInBounds(int& x, int& y, sf::Vector2i* appliedShift) {
    sf::Vector2i off{ 0, 0 };
    if (!inBounds(x, y)) {
        off = grow(x, y);
        x += off.x; y += off.y;
        pendingShift.x += off.x; pendingShift.y += off.y;
    }
    if (appliedShift) *appliedShift = off;
}

sf::Vector2i TileMap::grow(int newX, int newY) {
    const int dx = (newX < 0) ? roundUp(-newX, GROW_STEP) : 0;
    const int dy = (newY < 0) ? roundUp(-newY, GROW_STEP) : 0;

    const int oldW = (int)width, oldH = (int)height;
    const int newW = (newX + dx + 1 > oldW + dx) ? roundUp(newX + dx + 1, GROW_STEP) : oldW + dx;
    const int newH = (newY + dy + 1 > oldH + dy) ? roundUp(newY + dy + 1, GROW_STEP) : oldH + dy;

    std::vector<uint8_t>  newValue((size_t)(newW + 1) * (newH + 1), 0);
    std::vector<uint16_t> newWalls((size_t)newW * newH, 0);

    for (int y = 0; y < oldH; ++y) {
        std::copy_n(&value[(size_t)y * (oldW + 1)], oldW,
            &newValue[(size_t)(y + dy) * (newW + 1) + dx]);
        std::copy_n(&walls[(size_t)y * oldW], oldW,
            &newWalls[(size_t)(y + dy) * newW + dx]);
    }
    for (int x = 0; x <= newW; ++x) newValue[(size_t)newH * (newW + 1) + x] = TF_SOLID;
    for (int y = 0; y <= newH; ++y) newValue[(size_t)y * (newW + 1) + newW] = TF_SOLID;

    width = newW;
    height = newH;
    nbTiles = (uint32_t)newW * newH;
    value = std::move(newValue);
    walls = std::move(newWalls);
    ++version;
    return { dx, dy };
}