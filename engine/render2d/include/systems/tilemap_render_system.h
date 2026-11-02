#pragma once
// Rendu des tilemaps « data-driven » (tilemap/tilemap.h), orthogonales ou isométriques.
//
// Chaque case contient (tileset, tile) :
//  - une tile précise de la feuille  : elle est dessinée telle quelle ;
//  - « autotile »                    : la pièce est choisie selon les cases voisines :
//      Corners16 : feuille dual-grid, une pièce par sommet selon les 4 cases autour
//                  (orthogonal ; en isométrique, la pièce « pleine » de la feuille est utilisée) ;
//      Edges16   : la case pleine a son image ; sur les cases VOISINES d'un tileset placé plus bas
//                  dans la liste, des pièces « 1 bord / 2 bords / 4 bords » font la transition.
//  - Isométrique : cases dessinées de l'arrière vers l'avant (les blocs se recouvrent correctement).
//
// La liste des tilesets de la carte donne l'empilement : un autotile plus bas déborde sur ceux d'avant.
//
// Tilesets « tri en Y » (Tileset::ySort : arbres, murs...) : leurs tiles partent dans la YSortQueue de la
// cible et sont dessinées par le RenderSystem, mélangées aux entités selon leur Y (le perso passe devant
// ou derrière). Sans RenderSystem sur cette cible, elles sont dessinées tout de suite.
#include "components/transform2d.h"
#include "ecs/registry.h"
#include "ecs/system.h"
#include "tilemap/tilemap.h"
#include "utils/resource_loader.h"
#include "utils/ysort_queue.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>

class TilemapRenderer {
public:
    explicit TilemapRenderer(IResourceLoader& loader) : m_loader(loader) {}

    // `offset` : position de la carte dans le monde (Transform2D) ; `time` : secondes, pour les animations
    void draw(sf::RenderTarget& target, const TileMapData& map, sf::Vector2f offset, float time) {
        if (map.width() <= 0 || map.height() <= 0 || map.tilesets.empty()) return;
        begin(target, map, offset);
        m_queue = &YSortQueue::of(target);
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

    // Une tile seule (aperçu du pinceau dans l'éditeur). `tile` = kAutoTile : la pièce pleine du tileset.
    void drawGhost(sf::RenderTarget& target, const TileMapData& map, sf::Vector2f offset, int slot, int tile,
                   int x, int y, sf::Color color) {
        const Tileset* ts = map.tileset(slot);
        if (!ts || ts->missing) return;
        begin(target, map, offset);
        m_queue = nullptr;   // aperçu : dessiné tout de suite, par-dessus
        m_tint = color;
        if (tile == kAutoTile) {
            if (ts->autotile == AutotileMode::Edges16)
                for (int u : ts->under) { drawTile(*ts, u, x, y); break; }
            tile = ts->previewTile();
        }
        drawTile(*ts, tile, x, y);
        m_tint = sf::Color::White;
    }

private:
    void begin(sf::RenderTarget& target, const TileMapData& map, sf::Vector2f offset) {
        m_target = &target;
        m_map = &map;
        m_offset = offset;
    }

    static uint32_t hash3(int x, int y, int t) {
        uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(t) * 83492791u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return h;
    }

    const sf::Texture* texture(const Tileset& ts) {
        if (ts.image.empty()) return nullptr;
        const std::string full = m_map->resolve(ts.image);
        auto it = m_textures.find(full);
        if (it == m_textures.end()) it = m_textures.emplace(full, &m_loader.loadTexture(full)).first;
        return it->second;
    }

    // La tile `index` de la feuille, posée sur la case (x, y)
    void drawTile(const Tileset& ts, int index, int x, int y) {
        if (!ts.validTile(index)) return;
        const sf::Texture* tex = texture(ts);
        if (!tex) return;
        const sf::IntRect sub = ts.rect(index);
        if (sub.position.x + sub.size.x > int(tex->getSize().x) || sub.position.y + sub.size.y > int(tex->getSize().y))
            return;   // l'image a changé de taille : on ne dessine pas n'importe quoi
        sf::Sprite sprite(*tex, sub);
        sprite.setColor(m_tint);
        const sf::Vector2f center = m_map->cellCenter(x, y);
        if (m_map->orientation == TileMapOrientation::Orthogonal && ts.anchor.x < 0.f) {
            // Orthogonal sans ancre : la tile remplit la case
            sprite.setPosition(center - m_map->tileSize * 0.5f + m_offset);
            sprite.setScale({ m_map->tileSize.x / float(sub.size.x), m_map->tileSize.y / float(sub.size.y) });
        } else {
            const sf::Vector2f anchor = ts.anchor.x >= 0.f ? ts.anchor : sf::Vector2f(sub.size) * 0.5f;
            sprite.setOrigin(anchor);
            sprite.setPosition(center + m_offset);
            sprite.setScale({ ts.scale, ts.scale });
        }
        if (ts.ySort && m_queue && m_queue->hasConsumer()) {
            // point de tri : centre de la case (iso) ou bas de la case (orthogonal)
            float sortY = center.y + m_offset.y + ts.sortOffset;
            if (m_map->orientation == TileMapOrientation::Orthogonal) sortY += m_map->tileSize.y * 0.5f;
            m_queue->items.push_back({ sortY, sprite });
            return;
        }
        m_target->draw(sprite);
    }

    int pick(const std::vector<int>& list, uint32_t h) const {
        return list.empty() ? -1 : list[h % list.size()];
    }

    int row(const Tileset& ts, uint32_t hash) const {
        const int rows = std::max(1, ts.rows());
        if (ts.frameTime > 0.f) return int(m_time / ts.frameTime) % rows;   // animation
        return int((hash >> 8) % uint32_t(rows));                          // variante
    }

    // Case pleine d'un autotile (variante stable par case)
    void drawFull(const Tileset& ts, int slot, int x, int y) {
        const uint32_t h = hash3(x, y, slot);
        switch (ts.autotile) {
        case AutotileMode::Edges16:
            if (const int u = pick(ts.under, h >> 4); u >= 0) drawTile(ts, u, x, y);
            drawTile(ts, pick(ts.full, h), x, y);
            break;
        case AutotileMode::Corners16:   // colonne 15 = les 4 coins couverts
            drawTile(ts, ts.tileIndex(15, row(ts, h)), x, y);
            break;
        default:
            drawTile(ts, 0, x, y);
            break;
        }
    }

    void drawCell(int x, int y) {
        if (!m_visible.contains(m_map->cellCenter(x, y))) return;
        const TileCell c = m_map->get(x, y);
        const int slot = cellSlot(c);
        const Tileset* ts = m_map->tileset(slot);
        const bool ortho = m_map->orientation == TileMapOrientation::Orthogonal;

        if (ts && !ts->missing) {
            if (!cellIsAuto(c)) drawTile(*ts, cellTile(c), x, y);
            // en orthogonal, les Corners16 sont déjà dessinées par sommets
            else if (!(ortho && ts->autotile == AutotileMode::Corners16)) drawFull(*ts, slot, x, y);
        } else if (!ortho) {
            return;   // isométrique : rien sous une case vide, pas de transition à poser
        }

        // Transitions : les autotiles « 4 bords » placés APRÈS celui-ci débordent depuis les voisines
        for (int other = slot + 1; other < int(m_map->tilesets.size()); ++other) {
            const Tileset* t = m_map->tileset(other);
            if (!t || t->missing || t->autotile != AutotileMode::Edges16) continue;
            const TileCell terrain = makeCell(other, kAutoTile);
            int mask = 0;
            for (int i = 0; i < 4; ++i)
                if (m_map->get(x + kTileEdgeDirs[i].x, y + kTileEdgeDirs[i].y) == terrain) mask |= 1 << i;
            if (mask) drawEdges(*t, other, x, y, mask);
        }
    }

    // Pièces « bords » pour un masque de 4 bits (bit i = voisin i est de ce terrain)
    void drawEdges(const Tileset& t, int slot, int x, int y, int mask) {
        if (mask == 15 && !t.ring.empty()) {
            drawTile(t, pick(t.ring, hash3(x, y, slot)), x, y);
            return;
        }
        int left = mask;
        // deux bords adjacents -> une pièce « coin » (bord i + bord i+1)
        for (int i = 0; i < 4; ++i) {
            const int a = 1 << i, b = 1 << ((i + 1) % 4);
            if ((left & a) && (left & b) && t.validTile(t.corner[i])) {
                drawTile(t, t.corner[i], x, y);
                left &= ~(a | b);
            }
        }
        for (int i = 0; i < 4; ++i)
            if ((left & (1 << i)) && t.validTile(t.edge[i])) drawTile(t, t.edge[i], x, y);
    }

    // Orthogonal : feuilles dual-grid, une pièce par sommet selon les 4 cases qui l'entourent
    void drawCorners16(int x0, int y0, int x1, int y1) {
        const TileMapData& map = *m_map;
        bool any = false;
        for (const auto& t : map.tilesets) any |= t && !t->missing && t->autotile == AutotileMode::Corners16;
        if (!any) return;

        static const int dx[4] = { -1, 0, -1, 0 };
        static const int dy[4] = { -1, -1, 0, 0 };
        static const int bit[4] = { 8, 4, 2, 1 };
        const sf::Vector2f ts = map.tileSize;

        for (int vy = y0; vy <= y1 + 1; ++vy)
            for (int vx = x0; vx <= x1 + 1; ++vx) {
                // n° + 1 du tileset Corners16 de chaque case autour du sommet (0 sinon) ; hors carte = case du bord
                int ids[4];
                for (int i = 0; i < 4; ++i) {
                    const int cx = std::clamp(vx + dx[i], 0, map.width() - 1);
                    const int cy = std::clamp(vy + dy[i], 0, map.height() - 1);
                    const TileCell c = map.get(cx, cy);
                    const Tileset* t = map.tilesetOf(c);
                    ids[i] = (cellIsAuto(c) && t && !t->missing && t->autotile == AutotileMode::Corners16) ? cellSlot(c) + 1 : 0;
                }
                // du dessous vers le dessus : chaque tileset couvre les coins des tilesets de rang >= au sien
                int done = 0;
                for (int pass = 0; pass < 4; ++pass) {
                    int next = INT32_MAX;
                    for (int id : ids) if (id > done && id < next) next = id;
                    if (next == INT32_MAX) break;
                    done = next;
                    int mask = 0;
                    for (int i = 0; i < 4; ++i) if (ids[i] >= next) mask |= bit[i];
                    drawCornerPiece(*map.tileset(next - 1), next - 1, vx, vy, mask, ts);
                }
            }
    }

    void drawCornerPiece(const Tileset& t, int slot, int vx, int vy, int mask, sf::Vector2f ts) {
        const int index = t.tileIndex(mask, row(t, hash3(vx, vy, slot)));
        if (!t.validTile(index)) return;
        const sf::Texture* tex = texture(t);
        if (!tex) return;
        const sf::IntRect sub = t.rect(index);
        sf::Sprite sprite(*tex, sub);
        sprite.setColor(m_tint);
        sprite.setPosition(sf::Vector2f{ vx * ts.x - ts.x * 0.5f, vy * ts.y - ts.y * 0.5f } + m_offset);
        sprite.setScale({ ts.x / float(sub.size.x), ts.y / float(sub.size.y) });
        m_target->draw(sprite);
    }

    IResourceLoader& m_loader;
    std::unordered_map<std::string, const sf::Texture*> m_textures;
    sf::RenderTarget* m_target = nullptr;
    const TileMapData* m_map = nullptr;
    sf::Vector2f m_offset;
    sf::FloatRect m_visible;
    sf::Color m_tint = sf::Color::White;
    float m_time = 0.f;
    YSortQueue* m_queue = nullptr;
};

// Système ECS : dessine toutes les entités qui ont un composant Tilemap (sous les entités : priorité 5)
class TilemapRenderSystem : public ISystem {
public:
    TilemapRenderSystem(sf::RenderTarget& target, IResourceLoader& loader) : m_target(target), m_renderer(loader) {
        priority = 5;
    }

    void onUpdate(Registry& registry, float dt) override {
        m_time += dt;
        YSortQueue::of(m_target).items.clear();   // ce que le RenderSystem n'a pas dessiné (désactivé) est oublié
        registry.view<Tilemap>([&](Entity e, Tilemap& tm) {
            if (!tm.visible || !tm.data) return;
            const sf::Vector2f offset = registry.hasComponent<Transform2D>(e)
                ? registry.getComponent<Transform2D>(e).getPosition() : sf::Vector2f{};
            m_renderer.draw(m_target, *tm.data, offset, m_time);
        });
    }

    TilemapRenderer& renderer() { return m_renderer; }

private:
    sf::RenderTarget& m_target;
    TilemapRenderer m_renderer;
    float m_time = 0.f;
};
