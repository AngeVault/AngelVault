#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/collider2d.h"
#include "utils/collider_shape.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

class RenderCollisionSystem : public ISystem {
public:
	explicit RenderCollisionSystem(sf::RenderTarget& target) : m_target(target) {
        priority = 11; // s'exécute en dernier, après physique et script et après render
    }

    // Contours des collisions, exactement comme la physique les voit (utils/collider_shape.h) :
    // la forme et son décalage tournent avec l'entité. Rouge : solide, jaune : déclencheur.
    void onUpdate(Registry& registry, float dt) override
    {
        drawCollisionGrid();

        registry.view<Collider2D, Transform2D>([&](Entity, Collider2D& c, Transform2D& t) {
            const std::vector<sf::Vector2f> pts = collider_shape::worldOutline(c, t);
            if (pts.size() < 2) return;
            const sf::Color color = c.isTrigger ? sf::Color(255, 210, 60) : sf::Color::Red;
            m_outline.clear();
            for (const sf::Vector2f& p : pts) m_outline.append(sf::Vertex{ p, color });
            m_outline.append(sf::Vertex{ pts.front(), color });
            m_target.draw(m_outline);
        });
    }

private:
    sf::RenderTarget& m_target;
	sf::VertexArray m_gridLines{ sf::PrimitiveType::Lines };
	sf::VertexArray m_outline{ sf::PrimitiveType::LineStrip };

	void drawCollisionGrid() {
		constexpr float cellSize = 128.f; // identique à la grille spatiale de CollisionSystem
		const sf::View& view = m_target.getView();
		const sf::Vector2f viewSize = view.getSize();
		const sf::Vector2f viewCenter = view.getCenter();
		const float left = viewCenter.x - viewSize.x * 0.5f;
		const float right = viewCenter.x + viewSize.x * 0.5f;
		const float top = viewCenter.y - viewSize.y * 0.5f;
		const float bottom = viewCenter.y + viewSize.y * 0.5f;
		const sf::Color gridColor(125, 205, 225, 85);

		m_gridLines.clear();
		for (float x = std::floor(left / cellSize) * cellSize; x <= right; x += cellSize) {
			m_gridLines.append(sf::Vertex{ { x, top }, gridColor });
			m_gridLines.append(sf::Vertex{ { x, bottom }, gridColor });
		}
		for (float y = std::floor(top / cellSize) * cellSize; y <= bottom; y += cellSize) {
			m_gridLines.append(sf::Vertex{ { left, y }, gridColor });
			m_gridLines.append(sf::Vertex{ { right, y }, gridColor });
		}
		m_target.draw(m_gridLines);
	}
};
