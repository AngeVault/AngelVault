#pragma once
// Rendu des tilemaps « data-driven » (tilemap/tilemap.h), orthogonales ou isométriques.
//
//  - Tile simple      : son image (une variante au hasard, toujours la même pour une case donnée).
//  - Autotile 16 coins (Corners16) : feuille dual-grid, une pièce par sommet selon les 4 cases autour
//                       (orthogonal ; en isométrique, la pièce « pleine » de la feuille est utilisée).
//  - Autotile 4 bords (Edges16) : la case pleine a son image ; sur les cases VOISINES d'une tile placée
//                       plus bas dans la liste, des pièces « 1 bord / 2 bords / 4 bords » font la transition.
//  - Isométrique : cases dessinées de l'arrière vers l'avant (les blocs se recouvrent correctement).
//
// La liste des tiles donne l'empilement : une tile plus bas dans la liste déborde sur celles d'avant.
#include "components/transform2d.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "tilemap/tilemap.h"
#include "utils/resource_loader.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>

class TilemapRenderer {
public:
    explicit TilemapRenderer(IResourceLoader& loader) : m_loader(loader) {}

    // `offset` : position de la carte dans le monde (Transform2D) ; `time` : secondes, pour les animations
    void draw(sf::RenderTarget& target, const TileMapData& map, sf::Vector2f offset, float time) {
        if (map.width() <= 0 || map.height() <= 0 || map.tiles.empty()) return;
        m_target = &target;
        m_map = &map;
        m_offset = offset;
        m_time = time;

        // Partie visible, en coordonnées locales de la carte (avec une marge : les images dépassent des cases)
        const sf::View& view = target.getView();
        const float margin = 4.f * std::max(map.tileSize.x, map.tileSize.y);
        const sf::Vector2f c = view.getCenter() - offset;
        const sf::Vector2f half = view.getSize() * 0.5f + sf::Vector2f{ margin, margin };
        m_visible = sf::FloatRect(c - half, half * 2.f);

        // Cases candidates : boîte englobante des 4 coins de la vue, en coordonnées de cases
        int x0 = map.width(), y0 = map.height(), x1 = -1, y1 = -1;
        for (sf::Vector2f p : { c - half, sf::Vector2f{ c.x + half.x, c.y - half.y }, c + half,
                                sf::Vector2f{ c.x - half.x, c.y + half.y } }) {
            const sf::Vector2i cell = map.cellAt(p);
            x0 = std::min(x0, cell.x); y0 = std::min(y0, cell.y);
            x1 = std::max(x1, cell.x); y1 = std::max(y1, cell.y);
        }
        x0 = std::max(0, x0 - 1); y0 = std::max(0, y0 - 1);
        x1 = std::min(map.width() - 1, x1 + 1); y1 = std::min(map.height() - 1, y1 + 1);
        if (x1 < x0 || y1 < y0) return;

        if (map.orientation == TileMapOrientation::Isometric) {
            // De l'arrière (x + y petit) vers l'avant
            for (int s = x0 + y0; s <= x1 + y1; ++s)
                for (int x = std::max(x0, s - y1); x <= std::min(x1, s - y0); ++x) drawCell(x, s - x);
        } else {
            drawCorners16(x0, y0, x1, y1);
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) drawCell(x, y);
        }
    }

private:
    static uint32_t hash3(int x, int y, int t) {
        uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(t) * 83492791u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return h;
    }

    const sf::Texture* texture(const std::string& path) {
        if (path.empty()) return nullptr;
        const std::string full = m_map->resolve(path);
        auto it = m_textures.find(full);
        if (it == m_textures.end()) it = m_textures.emplace(full, &m_loader.loadTexture(full)).first;
        return it->second;
    }

    // Une image posée sur la case (x, y). `sub` : morceau de la texture (vide = toute l'image).
    void drawImage(const TileDef& def, const std::string& path, int x, int y, sf::IntRect sub = {}) {
        const sf::Texture* tex = texture(path);
        if (!tex) return;
        if (sub.size.x <= 0) sub = sf::IntRect({ 0, 0 }, sf::Vector2i(tex->getSize()));
        sf::Sprite sprite(*tex, sub);
        const sf::Vector2f center = m_map->cellCenter(x, y);
        if (m_map->orientation == TileMapOrientation::Orthogonal && def.anchor.x < 0.f) {
            // Orthogonal sans ancre : l'image remplit la case
            sprite.setPosition(center - m_map->tileSize * 0.5f + m_offset);
            sprite.setScale({ m_map->tileSize.x / float(sub.size.x), m_map->tileSize.y / float(sub.size.y) });
        } else {
            const sf::Vector2f anchor = def.anchor.x >= 0.f ? def.anchor : sf::Vector2f(sub.size) * 0.5f;
            sprite.setOrigin(anchor);
            sprite.setPosition(center + m_offset);
            sprite.setScale({ def.scale, def.scale });
        }
        m_target->draw(sprite);
    }

    // Image « pleine » d'une tile (variante stable par case)
    void drawFull(const TileDef& def, uint16_t id, int x, int y) {
        if (def.images.empty()) return;
        const uint32_t h = hash3(x, y, id);
        const std::string& path = def.images[h % def.images.size()];
        if (def.autotile == AutotileMode::Corners16) {   // colonne 15 = les 4 coins couverts
            const sf::Texture* tex = texture(path);
            if (!tex) return;
            const int cell = std::max(1, def.cellPixels);
            const int rows = std::max(1, int(tex->getSize().y) / cell);
            drawImage(def, path, x, y, sf::IntRect({ 15 * cell, row(def, rows, h) * cell }, { cell, cell }));
            return;
        }
        drawImage(def, path, x, y);
    }

    int row(const TileDef& def, int rows, uint32_t hash) const {
        if (def.frameTime > 0.f) return int(m_time / def.frameTime) % rows;   // animation
        return int((hash >> 8) % uint32_t(rows));                            // variante
    }

    void drawCell(int x, int y) {
        if (!m_visible.contains(m_map->cellCenter(x, y))) return;
        const uint16_t id = m_map->get(x, y);
        const TileDef* def = m_map->tile(id);
        const bool ortho = m_map->orientation == TileMapOrientation::Orthogonal;

        if (def) {
            // Dessous (ex. bloc de terre sous l'herbe)
            if (const uint16_t baseId = def->base.empty() ? 0 : m_map->tileId(def->base); baseId && baseId != id) {
                const TileDef* base = m_map->tile(baseId);
                if (base && !(ortho && base->autotile == AutotileMode::Corners16)) drawFull(*base, baseId, x, y);
            }
            // La case elle-même (en orthogonal, les Corners16 sont déjà dessinées par sommets)
            if (!(ortho && def->autotile == AutotileMode::Corners16)) drawFull(*def, id, x, y);
        } else if (!ortho) {
            return;   // isométrique : rien sous une case vide, pas de transition à poser
        }

        // Transitions : les tiles « 4 bords » placées APRÈS celle-ci débordent depuis les voisines
        for (uint16_t other = uint16_t(id + 1); other <= m_map->tiles.size(); ++other) {
            const TileDef& t = m_map->tiles[other - 1];
            if (t.autotile != AutotileMode::Edges16) continue;
            int mask = 0;
            for (int i = 0; i < 4; ++i)
                if (m_map->get(x + kTileEdgeDirs[i].x, y + kTileEdgeDirs[i].y) == other) mask |= 1 << i;
            if (mask) drawEdges(t, other, x, y, mask);
        }
    }

    // Pièces « bords » pour un masque de 4 bits (bit i = voisin i est de cette tile)
    void drawEdges(const TileDef& t, uint16_t id, int x, int y, int mask) {
        if (mask == 15 && !t.ring.empty()) {
            drawImage(t, t.ring[hash3(x, y, id) % t.ring.size()], x, y);
            return;
        }
        int left = mask;
        // deux bords adjacents -> une pièce « coin » (bord i + bord i+1)
        for (int i = 0; i < 4; ++i) {
            const int a = 1 << i, b = 1 << ((i + 1) % 4);
            if ((left & a) && (left & b) && !t.corner[i].empty()) {
                drawImage(t, t.corner[i], x, y);
                left &= ~(a | b);
            }
        }
        for (int i = 0; i < 4; ++i)
            if ((left & (1 << i)) && !t.edge[i].empty()) drawImage(t, t.edge[i], x, y);
    }

    // Orthogonal : feuilles dual-grid, une pièce par sommet selon les 4 cases qui l'entourent
    void drawCorners16(int x0, int y0, int x1, int y1) {
        const TileMapData& map = *m_map;
        bool any = false;
        for (const TileDef& t : map.tiles) any |= t.autotile == AutotileMode::Corners16;
        if (!any) return;

        static const int dx[4] = { -1, 0, -1, 0 };
        static const int dy[4] = { -1, -1, 0, 0 };
        static const int bit[4] = { 8, 4, 2, 1 };
        const sf::Vector2f ts = map.tileSize;

        for (int vy = y0; vy <= y1 + 1; ++vy)
            for (int vx = x0; vx <= x1 + 1; ++vx) {
                // id « Corners16 » de chaque case autour du sommet (0 sinon) ; hors carte = case du bord
                uint16_t ids[4];
                for (int i = 0; i < 4; ++i) {
                    const int cx = std::clamp(vx + dx[i], 0, map.width() - 1);
                    const int cy = std::clamp(vy + dy[i], 0, map.height() - 1);
                    const uint16_t id = map.get(cx, cy);
                    const TileDef* d = map.tile(id);
                    ids[i] = (d && d->autotile == AutotileMode::Corners16) ? id : 0;
                }
                // du dessous vers le dessus : chaque tile couvre les coins des tiles de rang >= au sien
                uint16_t done = 0;
                for (int pass = 0; pass < 4; ++pass) {
                    uint16_t next = 0xFFFF;
                    for (uint16_t id : ids) if (id > done && id < next) next = id;
                    if (next == 0xFFFF) break;
                    done = next;
                    int mask = 0;
                    for (int i = 0; i < 4; ++i) if (ids[i] >= next) mask |= bit[i];
                    drawCornerPiece(map.tiles[next - 1], next, vx, vy, mask, ts);
                }
            }
    }

    void drawCornerPiece(const TileDef& def, uint16_t id, int vx, int vy, int mask, sf::Vector2f ts) {
        if (def.images.empty()) return;
        const uint32_t h = hash3(vx, vy, id);
        const std::string& path = mask == 15 ? def.images[h % def.images.size()] : def.images[0];
        const sf::Texture* tex = texture(path);
        if (!tex) return;
        const int cell = std::max(1, def.cellPixels);
        const int rows = std::max(1, int(tex->getSize().y) / cell);
        sf::Sprite sprite(*tex, sf::IntRect({ mask * cell, row(def, rows, h) * cell }, { cell, cell }));
        sprite.setPosition(sf::Vector2f{ vx * ts.x - ts.x * 0.5f, vy * ts.y - ts.y * 0.5f } + m_offset);
        sprite.setScale({ ts.x / cell, ts.y / cell });
        m_target->draw(sprite);
    }

    IResourceLoader& m_loader;
    std::unordered_map<std::string, const sf::Texture*> m_textures;
    sf::RenderTarget* m_target = nullptr;
    const TileMapData* m_map = nullptr;
    sf::Vector2f m_offset;
    sf::FloatRect m_visible;
    float m_time = 0.f;
};

// Système ECS : dessine toutes les entités qui ont un composant Tilemap (sous les entités : priorité 5)
class TilemapRenderSystem : public ISystem {
public:
    TilemapRenderSystem(sf::RenderTarget& target, IResourceLoader& loader) : m_target(target), m_renderer(loader) {
        priority = 5;
    }

    void onUpdate(Registry& registry, float dt) override {
        m_time += dt;
        registry.view<Tilemap>([&](Entity e, Tilemap& tm) {
            if (!tm.visible || !tm.data) return;
            const sf::Vector2f offset = registry.hasComponent<Transform2D>(e)
                ? registry.getComponent<Transform2D>(e).getPosition() : sf::Vector2f{};
            m_renderer.draw(m_target, *tm.data, offset, m_time);
        });
    }

private:
    sf::RenderTarget& m_target;
    TilemapRenderer m_renderer;
    float m_time = 0.f;
};
