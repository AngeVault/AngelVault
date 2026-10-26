#pragma once
#include "ecs/system.h"
#include "RtsTile_map.hpp"
#include "components/camera2d.h"
#include "utils/resource_loader.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>


class TileMapRenderSystem : public ISystem {
public:
    TileMapRenderSystem(sf::RenderTarget& target, TileMap& map,
        Camera2d& camera, IResourceLoader& loader)
        : m_target(target), m_map(map), m_camera(camera), m_loader(loader) {
        priority = 5;
    }

    void invalidateTextures() { m_texturesReady = false; m_cacheValid = false; }

    void onUpdate(Registry&, float) override {
        if (!m_texturesReady || m_layerVersion != m_map.layerVersion) resolveTextures();

        const sf::Vector2u win = m_target.getSize();
        const float halfW = win.x * 0.5f / m_camera.zoom;
        const float halfH = win.y * 0.5f / m_camera.zoom;
        const float inv = 1.f / (float)m_map.tileSize;

        Region vis;
        vis.x0 = std::max(0, (int)std::floor((m_camera.position.x - halfW) * inv) - 1);
        vis.y0 = std::max(0, (int)std::floor((m_camera.position.y - halfH) * inv) - 1);
        vis.x1 = std::min((int)m_map.width, (int)std::floor((m_camera.position.x + halfW) * inv) + 2);
        vis.y1 = std::min((int)m_map.height, (int)std::floor((m_camera.position.y + halfH) * inv) + 2);
        if (vis.x1 <= vis.x0 || vis.y1 <= vis.y0) return;

        const bool mapChanged = !m_cacheValid || m_builtVersion != m_map.version;
        if (mapChanged || !m_built.contains(vis)) {
            m_built.x0 = std::max(0, vis.x0 - MARGIN);
            m_built.y0 = std::max(0, vis.y0 - MARGIN);
            m_built.x1 = std::min((int)m_map.width, vis.x1 + MARGIN);
            m_built.y1 = std::min((int)m_map.height, vis.y1 + MARGIN);

            build(false);
            build(true);
            m_builtVersion = m_map.version;
            m_builtAnim = m_map.animVersion;
            m_cacheValid = true;
        }
        else if (m_builtAnim != m_map.animVersion) {
            build(true);      // seuls les batches animés sont refaits
            m_builtAnim = m_map.animVersion;
        }

        sf::RenderStates states;
        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (m_batches[i].empty()) continue;
            states.texture = m_slots[i].tex;
            m_target.draw(m_batches[i].data(), m_batches[i].size(),
                sf::PrimitiveType::Triangles, states);
        }
    }

private:
    struct Region {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // [x0,x1) [y0,y1) en tiles
        bool contains(const Region& o) const {
            return o.x0 >= x0 && o.y0 >= y0 && o.x1 <= x1 && o.y1 <= y1;
        }
    };
    struct Slot {                     // une texture d'un type, dans l'ordre d'empilement
        const sf::Texture* tex = nullptr;
        int  rows = 1;
        const TileAnimation* anim = nullptr;   // null = texture statique (lignes = variantes)
        bool animated() const { return anim != nullptr; }
    };
    using Batch = std::vector<sf::Vertex>;

    static constexpr int MARGIN = 8;

    static uint32_t hash3(int x, int y, int t) {
        uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)t * 83492791u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return h;
    }

    void resolveTextures() {
        m_slots.clear();
        m_typeSlots.assign(256, {});
        const int cell = std::max(1, m_map.tileset.cellSize);

        for (TileType tt : m_map.layerOrder) {     
            const int type = (int)tt;
            auto it = m_map.tileset.variants.find(type);
            if (it == m_map.tileset.variants.end()) continue;
            const auto typeAnim = m_map.tileset.animations.find(type);
            for (const auto& id : it->second) {
                Slot s;
                sf::Texture& texture = m_loader.loadTexture(id);
                // Les spritesheets de terrain utilisent des masques pixel-art :
                // le filtrage bilinéaire peut mélanger leurs bords transparents
                // et faire apparaître des traits sombres entre les tiles.
                texture.setSmooth(false);
                s.tex = &texture;
                s.rows = std::max<int>(1, (int)s.tex->getSize().y / cell);
                const auto texAnim = m_map.tileset.textureAnimations.find(id);
                if (texAnim != m_map.tileset.textureAnimations.end()) s.anim = &texAnim->second;
                else if (typeAnim != m_map.tileset.animations.end())  s.anim = &typeAnim->second;
                m_typeSlots[type].push_back((int)m_slots.size());
                m_slots.push_back(s);
            }
        }
        m_batches.assign(m_slots.size(), Batch{});
        m_layerVersion = m_map.layerVersion;
        m_texturesReady = true;
        m_cacheValid = false;
    }

    void build(bool animatedPass) {
        for (size_t i = 0; i < m_slots.size(); ++i)
            if (m_slots[i].animated() == animatedPass) m_batches[i].clear();

        const float ts = (float)m_map.tileSize;
        const float half = ts * 0.5f;
        const float cell = (float)std::max(1, m_map.tileset.cellSize);
        const float k = cell / ts;                     
        const float mapW = m_map.width * ts, mapH = m_map.height * ts;

        const int vx1 = std::min(m_built.x1, (int)m_map.width);
        const int vy1 = std::min(m_built.y1, (int)m_map.height);

        VertexPiece pcs[4];
        for (int vy = m_built.y0; vy <= vy1; ++vy) {
            for (int vx = m_built.x0; vx <= vx1; ++vx) {
                const int n = m_map.vertexPieces(vx, vy, pcs);
                for (int p = 0; p < n; ++p) {
                    const int type = (int)pcs[p].type;
                    const auto& typeSlots = m_typeSlots[type];
                    if (typeSlots.empty()) continue;

                    const uint32_t h = hash3(vx, vy, type);
                    const int slotIdx = (pcs[p].mask == 15) ? typeSlots[h % typeSlots.size()] : typeSlots[0];
                    const Slot& s = m_slots[slotIdx];
                    if (s.animated() != animatedPass) continue;

                    // ligne : frame (animé) ou variante (statique)
                    const int row = s.animated() ? s.anim->currentFrame % s.rows
                        : (int)((h >> 8) % (uint32_t)s.rows);


                    // Léger recouvrement pour éviter les interstices d'un pixel
                    // quand le zoom transforme les coordonnées du monde en écran.
                    constexpr float seamOverlap = 0.75f;
                    float x0 = vx * ts - half, x1 = vx * ts + half;
                    float y0 = vy * ts - half, y1 = vy * ts + half;
                    if (x0 > 0.f) x0 -= seamOverlap;
                    if (x1 < mapW) x1 += seamOverlap;
                    if (y0 > 0.f) y0 -= seamOverlap;
                    if (y1 < mapH) y1 += seamOverlap;
                    float u0 = pcs[p].mask * cell, u1 = u0 + cell;
                    float v0 = row * cell, v1 = v0 + cell;

                    // on rogne ce qui dépasse de la carte
                    if (x0 < 0.f) { u0 += -x0 * k;          x0 = 0.f; }
                    if (x1 > mapW) { u1 -= (x1 - mapW) * k;  x1 = mapW; }
                    if (y0 < 0.f) { v0 += -y0 * k;          y0 = 0.f; }
                    if (y1 > mapH) { v1 -= (y1 - mapH) * k;  y1 = mapH; }

                    sf::Vertex a({ x0, y0 }, sf::Color::White, { u0, v0 });
                    sf::Vertex b({ x1, y0 }, sf::Color::White, { u1, v0 });
                    sf::Vertex d({ x1, y1 }, sf::Color::White, { u1, v1 });
                    sf::Vertex e({ x0, y1 }, sf::Color::White, { u0, v1 });

                    Batch& out = m_batches[slotIdx];
                    out.push_back(a); out.push_back(b); out.push_back(d);
                    out.push_back(a); out.push_back(d); out.push_back(e);
                }
            }
        }
    }

    sf::RenderTarget& m_target;
    TileMap& m_map;
    Camera2d& m_camera;
    IResourceLoader& m_loader;

    std::vector<Slot> m_slots;           
    std::vector<std::vector<int>> m_typeSlots; 
    std::vector<Batch> m_batches;         

    Region   m_built;
    uint32_t m_builtVersion = 0, m_builtAnim = 0, m_layerVersion = 0;
    bool     m_cacheValid = false, m_texturesReady = false;
};
