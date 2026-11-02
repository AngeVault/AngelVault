#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/geometry.h"
#include "components/unitComp.h"
#include "components/camera2d.h"
#include "utils/texture_factory.h"
#include "utils/loose_file_loader.h"
#include "utils/ysort_queue.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <string>

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

    void onUpdate(Registry& registry, float dt) override {
        struct Entry { int layer; float y; Geometry* geo; sf::Transform transform; const sf::Sprite* tile; };
        std::vector<Entry> drawList;

        registry.view<Transform2D, Geometry>([&](Entity e, Transform2D& t, Geometry& g) {
            if (registry.hasComponent<ECS::Unit>(e)) {
                const ECS::Unit& unit = registry.getComponent<ECS::Unit>(e);
                if (unit.team == ECS::Unit::Team::Enemy && !unit.visibleToPlayer) return;
            }
            drawList.push_back({ g.layer, sortY(t, g), &g, t.getLocalMatrix(), nullptr });
            });

        // Tiles « tri en Y » déposées par le TilemapRenderSystem (arbres, murs...)
        YSortQueue& queue = YSortQueue::of(m_target);
        for (const YSortQueue::Item& item : queue.items)
            drawList.push_back({ YSortQueue::kEntityLayer, item.y, nullptr, sf::Transform::Identity, &item.sprite });

        std::stable_sort(drawList.begin(), drawList.end(), [](const Entry& a, const Entry& b) {
            return a.layer != b.layer ? a.layer < b.layer : a.y < b.y;
        });

        for (auto& entry : drawList) {
            if (entry.tile) { m_target.draw(*entry.tile); continue; }
            std::visit([&](auto& shapeData) {
                using T = std::decay_t<decltype(shapeData)>;
				if constexpr (std::is_same_v<T, Geometry::SpriteData>) 
				{
					m_sprite.setTexture(m_textureLoader.loadTexture(shapeData.textureId));
					m_sprite.setTextureRect(shapeData.rect);
					m_sprite.setColor(shapeData.tint);
					m_sprite.setOrigin({ shapeData.rect.size.x / 2.f, shapeData.rect.size.y / 2.f });
					m_target.draw(m_sprite, entry.transform);

				}
                else if constexpr (std::is_same_v<T, Geometry::BoxData>) 
                {
					m_rect.setSize(shapeData.halfSize * 2.f);
					m_rect.setOrigin(shapeData.halfSize);
					m_rect.setFillColor(shapeData.style.fillColor);
					m_rect.setOutlineColor(shapeData.style.outlineColor);
					m_rect.setOutlineThickness(shapeData.style.outlineThickness);
                    if (shapeData.style.textureId != "")
                    {
					    m_rect.setTexture(& m_textureLoader.loadTexture(shapeData.style.textureId));
                    }
					else
					{
						m_rect.setTexture(nullptr);
					}
					m_target.draw(m_rect, entry.transform);
                }
				else if constexpr (std::is_same_v<T, Geometry::CircleData>)
				{
					sf::CircleShape circle(shapeData.radius);
					circle.setOrigin({ shapeData.radius, shapeData.radius });
					circle.setFillColor(shapeData.style.fillColor);
					circle.setOutlineColor(shapeData.style.outlineColor);
					circle.setOutlineThickness(shapeData.style.outlineThickness);
					if (shapeData.style.textureId != "")
					{
						circle.setTexture(&m_textureLoader.loadTexture(shapeData.style.textureId));
					}
					else
					{
						circle.setTexture(nullptr);
					}
					m_target.draw(circle, entry.transform);
				}
				else if constexpr (std::is_same_v<T, Geometry::PolygonData>)
				{
					sf::ConvexShape polygon;
					polygon.setPointCount(shapeData.points.size());
					for (size_t i = 0; i < shapeData.points.size(); ++i) {
						polygon.setPoint(i, shapeData.points[i]);
					}
					polygon.setFillColor(shapeData.style.fillColor);
					polygon.setOutlineColor(shapeData.style.outlineColor);
					polygon.setOutlineThickness(shapeData.style.outlineThickness);
					if (shapeData.style.textureId != "")
					{
						polygon.setTexture(&m_textureLoader.loadTexture(shapeData.style.textureId));
					}
					else
					{
						polygon.setTexture(nullptr);
					}
					m_target.draw(polygon, entry.transform);
				}
            }, entry.geo->data);
        }
        queue.items.clear();
    }

private:
    sf::RenderTarget& m_target;
	sf::Texture dummyTexture;
    sf::Sprite m_sprite{ dummyTexture };
	sf::RectangleShape m_rect;
	sf::CircleShape m_circle;
	sf::ConvexShape m_polygon;
	LooseFileLoader& m_textureLoader;
};

inline void test(Camera2d& cam)   // inline : ce header est inclus par plusieurs .cpp
{
	return;
}
