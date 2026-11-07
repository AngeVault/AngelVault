#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/geometry.h"
#include "utils/loose_file_loader.h"
#include "utils/ysort_queue.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

// Dessine les entités (Transform2D + Geometry), triées par calque (Geometry::layer) puis par Y :
// sur un même calque, ce qui est plus bas à l'écran (les « pieds ») passe devant.
// Les tiles « tri en Y » des tilemaps (YSortQueue) sont mélangées à ce tri.
class RenderSystem : public ISystem {
public:
    explicit RenderSystem(sf::RenderTarget& target, LooseFileLoader& textureLoader) : m_target(target), m_textureLoader(textureLoader) {
        priority = 10; // s'exécute en dernier, après physique et script
        ++YSortQueue::of(m_target).consumers;
    }
    ~RenderSystem() override { --YSortQueue::of(m_target).consumers; }

    // Point de tri d'une entité : le bas de sa forme (ses pieds), dans le monde
    static float sortY(const Transform2D& t, const Geometry& g) {
        float bottom = 0.f;
        std::visit([&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, Geometry::SpriteData>) bottom = d.rect.size.y * 0.5f;
            else if constexpr (std::is_same_v<T, Geometry::BoxData>) bottom = d.halfSize.y;
            else if constexpr (std::is_same_v<T, Geometry::CircleData>) bottom = d.radius;
            else for (const sf::Vector2f& p : d.points) bottom = std::max(bottom, p.y);
        }, g.data);
        return t.getPosition().y + bottom * std::abs(t.getScale().y);
    }

    // rectangle de la forme avant le Transform2D (contour compris)
    static sf::FloatRect localBounds(const Geometry& g) {
        return std::visit([](const auto& d) -> sf::FloatRect {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, Geometry::SpriteData>) {
                const sf::Vector2f size(float(std::abs(d.rect.size.x)), float(std::abs(d.rect.size.y)));
                return { -size * 0.5f, size };
            } else {
                sf::FloatRect r;
                if constexpr (std::is_same_v<T, Geometry::BoxData>) r = { -d.halfSize, d.halfSize * 2.f };
                else if constexpr (std::is_same_v<T, Geometry::CircleData>) r = { { -d.radius, -d.radius }, { 2.f * d.radius, 2.f * d.radius } };
                else {
                    if (d.points.empty()) return {};
                    sf::Vector2f mn = d.points[0], mx = d.points[0];
                    for (const sf::Vector2f& p : d.points) {
                        mn = { std::min(mn.x, p.x), std::min(mn.y, p.y) };
                        mx = { std::max(mx.x, p.x), std::max(mx.y, p.y) };
                    }
                    r = { mn, mx - mn };
                }
                const float o = std::abs(d.style.outlineThickness);
                return { r.position - sf::Vector2f{ o, o }, r.size + sf::Vector2f{ 2.f * o, 2.f * o } };
            }
        }, g.data);
    }

    void onUpdate(Registry& registry, float) override {
        const sf::FloatRect visible = viewBounds();
        m_drawList.clear();
        registry.view<Transform2D, Geometry>([&](Entity, Transform2D& t, Geometry& g) {
            const sf::Transform tf = t.getLocalMatrix();
            if (!tf.transformRect(localBounds(g)).findIntersection(visible)) return;   // hors champ
            m_drawList.push_back({ g.layer, sortY(t, g), &g, tf, nullptr });
        });

        // Tiles « tri en Y » déposées par le TilemapRenderSystem (arbres, murs...)
        YSortQueue& queue = YSortQueue::of(m_target);
        for (const YSortQueue::Item& item : queue.items)
            m_drawList.push_back({ YSortQueue::kEntityLayer, item.y, nullptr, sf::Transform::Identity, &item.sprite });

        std::stable_sort(m_drawList.begin(), m_drawList.end(), [](const Entry& a, const Entry& b) {
            return a.layer != b.layer ? a.layer < b.layer : a.y < b.y;
        });

        for (const Entry& entry : m_drawList) {
            if (entry.tile) { m_target.draw(*entry.tile); continue; }
            std::visit([&](const auto& d) { drawShape(d, entry.transform); }, entry.geo->data);
        }
        queue.items.clear();
    }

private:
    struct Entry { int layer; float y; const Geometry* geo; sf::Transform transform; const sf::Sprite* tile; };

    // zone vue par la caméra, dans le monde (rotation comprise)
    sf::FloatRect viewBounds() const {
        return m_target.getView().getInverseTransform().transformRect({ { -1.f, -1.f }, { 2.f, 2.f } });
    }

    void applyStyle(sf::Shape& shape, const Geometry::ShapeStyle& style) {
        shape.setFillColor(style.fillColor);
        shape.setOutlineColor(style.outlineColor);
        shape.setOutlineThickness(style.outlineThickness);
        // resetRect : la forme est partagée, le rect doit suivre la texture de chaque entité
        if (style.textureId.empty()) shape.setTexture(nullptr);
        else shape.setTexture(&m_textureLoader.loadTexture(style.textureId), true);
    }

    void drawShape(const Geometry::SpriteData& d, const sf::Transform& tf) {
        m_sprite.setTexture(m_textureLoader.loadTexture(d.textureId));
        m_sprite.setTextureRect(d.rect);
        m_sprite.setColor(d.tint);
        m_sprite.setOrigin({ d.rect.size.x / 2.f, d.rect.size.y / 2.f });
        m_target.draw(m_sprite, tf);
    }

    void drawShape(const Geometry::BoxData& d, const sf::Transform& tf) {
        m_rect.setSize(d.halfSize * 2.f);
        m_rect.setOrigin(d.halfSize);
        applyStyle(m_rect, d.style);
        m_target.draw(m_rect, tf);
    }

    void drawShape(const Geometry::CircleData& d, const sf::Transform& tf) {
        m_circle.setRadius(d.radius);
        m_circle.setOrigin({ d.radius, d.radius });
        applyStyle(m_circle, d.style);
        m_target.draw(m_circle, tf);
    }

    void drawShape(const Geometry::PolygonData& d, const sf::Transform& tf) {
        if (d.points.size() < 3) return;
        m_polygon.setPointCount(d.points.size());
        for (std::size_t i = 0; i < d.points.size(); ++i) m_polygon.setPoint(i, d.points[i]);
        applyStyle(m_polygon, d.style);
        m_target.draw(m_polygon, tf);
    }

    sf::RenderTarget& m_target;
    LooseFileLoader& m_textureLoader;
    std::vector<Entry> m_drawList;   // gardée d'une frame à l'autre

    // formes réutilisées pour toutes les entités
    sf::Texture m_dummyTexture;
    sf::Sprite m_sprite{ m_dummyTexture };
    sf::RectangleShape m_rect;
    sf::CircleShape m_circle;
    sf::ConvexShape m_polygon;
};
