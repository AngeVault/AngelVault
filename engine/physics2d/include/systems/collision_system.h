#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/collider2d.h"    
#include "components/rigid_body2d.h" 
#include "components/transform2d.h"
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cfloat>
#include <type_traits>


struct CollisionInfo {
    Entity       entityA, entityB;
    sf::Vector2f normal;     // direction du choc (de A vers B)
    float        depth;      // profondeur de pénétration
    bool         isTrigger;  // true = pas de réponse physique
    sf::Vector2f point;

};

class CollisionSystem : public ISystem {
public:
    // Callback appelé à chaque collision détectée
    std::function<void(const CollisionInfo&)> onCollision;

    CollisionSystem() { priority = 1; } // après Physics (0), avant Render (10)

    void onUpdate(Registry& registry, float dt) override {
        // Collecte toutes les entités collidables
        m_entries.clear();

        registry.view<Collider2D, Transform2D>(
            [&](Entity e, Collider2D& c, Transform2D& t) {
                RigidBody2D* rb = registry.hasComponent<RigidBody2D>(e) ? &registry.getComponent<RigidBody2D>(e) : nullptr;
                m_entries.push_back({ e, &c, &t, rb });
            });

        // Grille spatiale : seuls les colliders dont les boîtes englobantes
        // partagent une cellule arrivent jusqu'au test précis.
        constexpr float cellSize = 128.f;
        m_spans.clear();
        m_cellEntries.clear();
        for (size_t i = 0; i < m_entries.size(); ++i) {
            const sf::Vector2f position = m_entries[i].tr->getPosition();
            const float radius = boundingRadius(*m_entries[i].col);
            const int minX = static_cast<int>(std::floor((position.x - radius) / cellSize));
            const int maxX = static_cast<int>(std::floor((position.x + radius) / cellSize));
            const int minY = static_cast<int>(std::floor((position.y - radius) / cellSize));
            const int maxY = static_cast<int>(std::floor((position.y + radius) / cellSize));
            m_spans.push_back({ minX, maxX, minY, maxY });
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                    m_cellEntries.push_back({ cellKey(x, y), x, y, i });
        }
        std::sort(m_cellEntries.begin(), m_cellEntries.end(), [](const CellEntry& a, const CellEntry& b) {
            return a.key < b.key || (a.key == b.key && a.colliderIndex < b.colliderIndex);
        });

        for (size_t begin = 0; begin < m_cellEntries.size();) {
            size_t end = begin + 1;
            while (end < m_cellEntries.size() && m_cellEntries[end].key == m_cellEntries[begin].key)
                ++end;
            const int cellX = m_cellEntries[begin].x;
            const int cellY = m_cellEntries[begin].y;
            for (size_t a = begin; a < end; ++a) {
                for (size_t b = a + 1; b < end; ++b) {
                    const size_t i = m_cellEntries[a].colliderIndex;
                    const size_t j = m_cellEntries[b].colliderIndex;
                    // Une paire peut partager plusieurs cellules : son coin commun
                    // supérieur-gauche est son unique cellule de traitement.
                    if (cellX != std::max(m_spans[i].minX, m_spans[j].minX)
                        || cellY != std::max(m_spans[i].minY, m_spans[j].minY)) continue;

                    auto& A = m_entries[i];
                    auto& B = m_entries[j];

                    // Deux statiques ne se calculent jamais
                    bool aStatic = !A.rb || A.rb->isStatic;
                    bool bStatic = !B.rb || B.rb->isStatic;
                    if (aStatic && bStatic) continue;

                    CollisionInfo info;
                    if (detect(*A.col, *A.tr, *B.col, *B.tr, info)) {
                        info.entityA = A.e;
                        info.entityB = B.e;
                        info.isTrigger = A.col->isTrigger || B.col->isTrigger;

                        if (onCollision) onCollision(info);

                        if (!info.isTrigger)
                            resolve(info, A.rb, B.rb, *A.tr, *B.tr, *A.col, *B.col);
                    }
                }
            }
            begin = end;
        }
    }

private:
    struct ColEntry {
        Entity e;
        Collider2D* col;
        Transform2D* tr;
        RigidBody2D* rb;
    };
    struct GridSpan { int minX, maxX, minY, maxY; };
    struct CellEntry { uint64_t key; int x, y; size_t colliderIndex; };
    std::vector<ColEntry> m_entries;
    std::vector<GridSpan> m_spans;
    std::vector<CellEntry> m_cellEntries;

    static uint64_t cellKey(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
            | static_cast<uint32_t>(y);
    }

    static float boundingRadius(const Collider2D& collider) {
        const float offset = std::hypot(collider.offset.x, collider.offset.y);
        const float shapeRadius = std::visit([](const auto& shape) -> float {
            using Shape = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<Shape, Collider2D::Box>)
                return std::hypot(shape.halfSize.x, shape.halfSize.y);
            else if constexpr (std::is_same_v<Shape, Collider2D::Circle>)
                return shape.radius;
            else if constexpr (std::is_same_v<Shape, Collider2D::Ellipse>)
                return std::hypot(shape.radii.x, shape.radii.y);
            else if constexpr (std::is_same_v<Shape, Collider2D::Capsule>)
                return shape.radius + shape.height * 0.5f;
            else {
                float radius = 0.f;
                for (const sf::Vector2f point : shape.points)
                    radius = std::max(radius, std::hypot(point.x, point.y));
                return radius;
            }
        }, collider.shape);
        return shapeRadius + offset;
    }

	sf::Vector2f computeWorldPosition(const Collider2D& c, const Transform2D& t) 
    {
		float cosA = std::cos(t.getRotation().asRadians());
		float sinA = std::sin(t.getRotation().asRadians());
		return { t.getPosition().x + c.offset.x * cosA - c.offset.y * sinA,
				 t.getPosition().y + c.offset.x * sinA + c.offset.y * cosA };
	}

    /// DÉTECTION — dispatche selon les types de formes
    bool detect(const Collider2D& cA, const Transform2D& tA,
        const Collider2D& cB, const Transform2D& tB,
        CollisionInfo& info)
    {
		sf::Vector2f posA = computeWorldPosition(cA, tA);
		sf::Vector2f posB = computeWorldPosition(cB, tB);

        using T = Collider2D::ShapeType;
        T typeA = cA.type(), typeB = cB.type();

        if ((typeA == T::Box && typeB == T::Box) && ( tA.getRotation().asDegrees() < 0.01f && tB.getRotation().asDegrees() < 0.01f))
            return boxVsBox(cA, posA, cB, posB, info);
        if (typeA == T::Circle && typeB == T::Circle)
            return circleVsCircle(cA, posA, cB, posB, info);
        if (typeA == T::Box && typeB == T::Circle)
            return boxVsCircle(cA, tA, cB, tB, info);
        if (typeA == T::Circle && typeB == T::Box) {
            bool hit = boxVsCircle(cB, tB, cA, tA, info);
            info.normal = -info.normal; // inverser la normale
            return hit;
        }
        // Polygon vs anything → SAT
        return satTest(cA, tA, cB, tB, info);
    }

    /// BOX vs BOX (AABB)
    //tex:
        //$$\text{Vecteur distance : } \Delta x = x_B - x_A, \quad \Delta y = y_B - y_A$$
        //$$\text{Chevauchement : } o_x = h_{A,x} + h_{B,x} - |\Delta x|, \quad o_y = h_{A,y} + h_{B,y} - |\Delta y|$$
        //$$\text{Condition de collision : } (o_x > 0) \land (o_y > 0)$$
        //$$\text{Normale et Profondeur : } \vec{n} = \begin{cases} (\text{sgn}(\Delta x), 0) & \text{si } o_x < o_y \\ (0, \text{sgn}(\Delta y)) & \text{si } o_x \ge o_y \end{cases}, \quad d = \min(o_x, o_y)$$
    bool boxVsBox(const Collider2D& cA, sf::Vector2f posA,  const Collider2D& cB, sf::Vector2f posB, CollisionInfo& info)
    {
        auto& boxA = std::get<Collider2D::Box>(cA.shape);
        auto& boxB = std::get<Collider2D::Box>(cB.shape);

        float dx = posB.x - posA.x;
        float dy = posB.y - posA.y;
        float overlapX = boxA.halfSize.x + boxB.halfSize.x - std::abs(dx);
        float overlapY = boxA.halfSize.y + boxB.halfSize.y - std::abs(dy);

        if (overlapX <= 0.f || overlapY <= 0.f) return false;

        float minX = std::max(posA.x - boxA.halfSize.x, posB.x - boxB.halfSize.x);
        float maxX = std::min(posA.x + boxA.halfSize.x, posB.x + boxB.halfSize.x);
        float minY = std::max(posA.y - boxA.halfSize.y, posB.y - boxB.halfSize.y);
        float maxY = std::min(posA.y + boxA.halfSize.y, posB.y + boxB.halfSize.y);

        if (overlapX < overlapY) 
        {

			float sign = (dx < 0.f) ? -1.f : 1.f;
            info.normal = { sign, 0.f };
            info.depth = overlapX;

			// le point de contact est au milieu de la hauteur du chevauchement

			float pointX = (sign > 0.f) ? (posA.x + boxA.halfSize.x) : (posA.x - boxA.halfSize.x);
			float pointY = (minY + maxY) * 0.5f;
			info.point = { pointX, pointY };

        }
        else 
        {

            float sign = (dy < 0.f) ? -1.f : 1.f;
            info.normal = { 0.f, sign };
            info.depth = overlapY;

			float pointX = (minX + maxX) * 0.5f;
			float pointY = (sign > 0.f) ? (posA.y + boxA.halfSize.y) : (posA.y - boxA.halfSize.y);
			info.point = { pointX, pointY }; 
        }



        return true;
    }

    /// CIRCLE vs CIRCLE
    //tex:
    //$$\text{Vecteur distance : } \Delta x = x_B - x_A, \quad \Delta y = y_B - y_A$$
    //$$\text{Somme des rayons et Distance : } r = r_A + r_B, \quad \|\Delta \mathbf{p}\|^2 = \Delta x^2 + \Delta y^2$$
    //$$\text{Condition de collision : } \|\Delta \mathbf{p}\|^2 < r^2$$
    //$$\text{Normale et Profondeur : } \vec{n} = \frac{(\Delta x, \Delta y)}{\|\Delta \mathbf{p}\|}, \quad d = r - \|\Delta \mathbf{p}\|$$
    bool circleVsCircle(const Collider2D& cA, sf::Vector2f posA, const Collider2D& cB, sf::Vector2f posB, CollisionInfo& info)
    {
        auto& circA = std::get<Collider2D::Circle>(cA.shape);
        auto& circB = std::get<Collider2D::Circle>(cB.shape);

        float dx = posB.x - posA.x;
        float dy = posB.y - posA.y;
        float dist = std::hypot(dx, dy);
        float sum = circA.radius + circB.radius;

        if (dist >= sum) return false;

        if (dist < 1e-6f) // evite les division par zero si les deux cecle sont a la meme position
        {  
            info.normal = { 1.f, 0.f };
            info.depth = sum;
			info.point = posA; // point de contact arbitraire

        }
        else  // cas normal 
        { 
            info.normal = { dx / dist, dy / dist };
            info.depth = sum - dist;

			info.point = posA + info.normal * (circA.radius - info.depth * 0.5f); // point de contact sur le cercle A
        }
        return true;
    }

    /// BOX vs CIRCLE
    bool boxVsCircle(const Collider2D& cBox, const Transform2D& tBox, const Collider2D& cCirc, const Transform2D& tCirc, CollisionInfo& info)
    {
        auto& box = std::get<Collider2D::Box>(cBox.shape);
        auto& circ = std::get<Collider2D::Circle>(cCirc.shape);

        sf::Vector2f posBox = computeWorldPosition(cBox, tBox);
        sf::Vector2f posCirc = computeWorldPosition(cCirc, tCirc);

		// transforme en repere local 
		float dx = posCirc.x - posBox.x;
		float dy = posCirc.y - posBox.y;

		float angle = tBox.getRotation().asRadians();
		float cosA = std::cos(angle);
		float sinA = std::sin(angle);

		// rotation inverse pour passer en repere local
		float localX = dx * cosA + dy * sinA;
		float localY = -dx * sinA + dy * cosA;

		// point le plus proche sur le rectangle
		float procheX = std::clamp(localX, -box.halfSize.x, box.halfSize.x);
		float procheY = std::clamp(localY, -box.halfSize.y, box.halfSize.y);
        
		// distance dans le repert local
		float distX = localX - procheX;
		float distY = localY - procheY;

		float distSq = distX * distX + distY * distY;

		// colision si la distance au carré est inférieure au rayon au carré
        if (distSq > circ.radius * circ.radius)
        {
            return false;
        }

		float dist = std::sqrt(distSq);

		sf::Vector2f localNormal(0.f, 0.f);

		if (distSq < 1e-6f) // si le centre du cercle est à l'intérieur de la boîte, on prend la direction de pénétration minimale
        {
            float depthX = box.halfSize.x - std::abs(localX);
            float depthY = box.halfSize.y - std::abs(localY);

            if (depthX < depthY) {
                localNormal = { (localX >= 0.f) ? 1.f : -1.f, 0.f };
                info.depth = circ.radius + depthX;
                procheX = (localX >= 0.f) ? box.halfSize.x : -box.halfSize.x;
            }
            else {
                localNormal = { 0.f, (localY >= 0.f) ? 1.f : -1.f };
                info.depth = circ.radius + depthY;
                procheY = (localY >= 0.f) ? box.halfSize.y : -box.halfSize.y;
            }
        }
        else // le centre du cercle est à l'extérieur de la boîte
        {
            localNormal = sf::Vector2f(distX / dist, distY / dist);
            info.depth = circ.radius - dist;
        }

        // normal : Local -> Monde
        info.normal.x = localNormal.x * cosA - localNormal.y * sinA;
        info.normal.y = localNormal.x * sinA + localNormal.y * cosA;

        // point d'impact : Local -> Monde
        //    On applique la rotation au point local puis on ajoute la position de la boîte
        info.point.x = (procheX * cosA - procheY * sinA) + posBox.x;
        info.point.y = (procheX * sinA + procheY * cosA) + posBox.y;

        return true;
    }

    // ---------------------------------------------------------------
    // SAT (Separating Axis Theorem) — pour polygones convexes   -    au cas ou j'oublie https://youtu.be/dn0hUgsok9M?si=b-K1nz0KqeI6FTRS (simple / visuel), https://youtu.be/Zgf1DYrmSnk?si=NN7MsydwIuKxenwV (code)
    // Principe : si on trouve UN axe où les projections ne se chevauchent pas
    //            → pas de collision. Sinon → collision sur l'axe de plus faible chevauchement
    // ---------------------------------------------------------------
    bool satTest(const Collider2D& cA, const Transform2D& tA, const Collider2D& cB, const Transform2D& tB, CollisionInfo& info)
    {
        auto ptsA = getWorldPoints(cA, tA);
        auto ptsB = getWorldPoints(cB, tB);
        
		sf::Vector2f posA = computeWorldPosition(cA, tA);
        sf::Vector2f posB = computeWorldPosition(cB,tB);

        float    minDepth = FLT_MAX;
        sf::Vector2f bestNormal;

        // Teste les normales des arêtes de A et de B
        for (int pass = 0; pass < 2; ++pass) {
            auto& pts = (pass == 0) ? ptsA : ptsB;
            for (size_t i = 0; i < pts.size(); ++i) {
                sf::Vector2f edge = pts[(i + 1) % pts.size()] - pts[i];
                sf::Vector2f axis = { -edge.y, edge.x }; // normale perpendiculaire
                float len = std::hypot(axis.x, axis.y);
                if (len < 1e-6f) continue;
                axis = { axis.x / len, axis.y / len };

                auto [minA, maxA] = project(ptsA, axis);
                auto [minB, maxB] = project(ptsB, axis);

                float depth = std::min(maxA, maxB) - std::max(minA, minB);
                if (depth <= 0.f) return false; // axe séparateur trouvé

                if (depth < minDepth) {
                    minDepth = depth;
                    bestNormal = axis;
                    
                }
            }
        }

        // S'assurer que la normale pointe de A vers B
        sf::Vector2f dir = posB - posA;
        if (dir.x * bestNormal.x + dir.y * bestNormal.y < 0.f)
            bestNormal = { -bestNormal.x, -bestNormal.y };

        info.normal = bestNormal;
        info.depth = minDepth;

		float maxProj = -FLT_MAX;
		sf::Vector2f contactPoint = ptsA[0];
		for (const auto& p : ptsA) 
        {
			float proj = p.x * bestNormal.x + p.y * bestNormal.y;
			if (proj > maxProj) {
				maxProj = proj;
				contactPoint = p;
			}
		}

		info.point = contactPoint + bestNormal * (info.depth * 0.5f);

        return true;
    }

    // Projette tous les points sur un axe, retourne [min, max]
    std::pair<float, float> project(const std::vector<sf::Vector2f>& pts, sf::Vector2f axis) {
        float mn = FLT_MAX, mx = -FLT_MAX;
        for (auto& p : pts) {
            float d = p.x * axis.x + p.y * axis.y;
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        return { mn, mx };
    }

    // Retourne les points mondiaux d'une forme (transformés en world space)
    std::vector<sf::Vector2f> getWorldPoints(const Collider2D& c, const Transform2D& t) 
    {
        return std::visit([&](auto& shape) -> std::vector<sf::Vector2f> 
            {
                float cosA = std::cos(t.getRotation().asRadians());
                float sinA = std::sin(t.getRotation().asRadians());

                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, Collider2D::Box>) 
                {
                    auto toWorld = [&](sf::Vector2f local) 
                        {
                            sf::Vector2f withOffset = local + c.offset; // offset et forme tournent ensemble
                            return sf::Vector2f
                            {
                                t.getPosition().x + withOffset.x * cosA - withOffset.y * sinA,
                                t.getPosition().y + withOffset.x * sinA + withOffset.y * cosA
                            };
                        };

                    return {
                        toWorld({ -shape.halfSize.x, -shape.halfSize.y }),
                        toWorld({  shape.halfSize.x, -shape.halfSize.y }),
                        toWorld({  shape.halfSize.x,  shape.halfSize.y }),
                        toWorld({ -shape.halfSize.x,  shape.halfSize.y })
                    };
                }
                if constexpr (std::is_same_v<T, Collider2D::Polygon>) {
                    std::vector<sf::Vector2f> out;
                    out.reserve(shape.points.size());
                    for (auto& p : shape.points)
						out.push_back({ t.getPosition().x + (p.x + c.offset.x) * cosA - (p.y + c.offset.y) * sinA,
										t.getPosition().y + (p.x + c.offset.x) * sinA + (p.y + c.offset.y) * cosA
                            });
                    return out;
                }
                // Fallback pour cercle/ellipse/capsule : approximation polygonale
                std::vector<sf::Vector2f> pts;
                float r = 16.f;
                if constexpr (std::is_same_v<T, Collider2D::Circle>) {
                    r = shape.radius;
                }
                else if constexpr (std::is_same_v<T, Collider2D::Ellipse>) {
                    r = std::max(shape.radii.x, shape.radii.y);
                }
                else if constexpr (std::is_same_v<T, Collider2D::Capsule>) {
                    r = shape.radius;
                }
                for (int i = 0; i < 8; ++i) {
                    float a = i * 3.14159f * 2.f / 8.f;
					sf::Vector2f p = { std::cos(a) * r, std::sin(a) * r };
                    pts.push_back({ t.getPosition().x + (p.x + c.offset.x) * cosA - (p.y + c.offset.y) * sinA  ,
                        t.getPosition().y + (p.x + c.offset.x) * sinA + (p.y + c.offset.y) * cosA 
                        });
                }
                return pts;
            }, c.shape);
    }

    // ---------------------------------------------------------------
    // RÉSOLUTION — applique l'impulsion et corrige la pénétration
    // ---------------------------------------------------------------
    void resolve(const CollisionInfo& info, RigidBody2D* rbA, RigidBody2D* rbB, Transform2D& tA, Transform2D& tB, const Collider2D& cA, const Collider2D& cB)
    {
        float invMassA = rbA ? rbA->inverseMass() : 0.f;
        float invMassB = rbB ? rbB->inverseMass() : 0.f;
        float totalInv = invMassA + invMassB;
        if (totalInv < 1e-6f) return;

        // pénétration
        const float percent = 1.0f;
        const float slop = 0.005f;
        float       correction = std::max(info.depth - slop, 0.f) / totalInv * percent;
        sf::Vector2f corr = info.normal * correction;
        if (rbA && !rbA->isStatic) tA.move(-corr * invMassA);
        if (rbB && !rbB->isStatic) tB.move(corr * invMassB);

        // Détection du sol (pour le saut)
        const float groundThreshold = 0.5f;
        if (info.normal.y > groundThreshold)
        {
            if (rbA)
            {
                rbA->isGrounded = true;
            }
        }
        else if (info.normal.y < -groundThreshold)
        {
            if (rbB) {
                rbB->isGrounded = true;
            }
        }

        //  Calcul de l'impulsion 
        sf::Vector2f velA = rbA ? rbA->velocity : sf::Vector2f{};
        sf::Vector2f velB = rbB ? rbB->velocity : sf::Vector2f{};
        sf::Vector2f relVel = velB - velA;
        float velAlongNormal = relVel.x * info.normal.x + relVel.y * info.normal.y;

        if (velAlongNormal > 0.f) return;

        // Prend le MAX des deux restitutions
        float e = std::max(cA.restitution, cB.restitution);

        // Seuil très bas pour éviter les micro-vibrations au repos uniquement
        if (std::abs(velAlongNormal) < 0.05f) e = 0.f;

        float j = -(1.f + e) * velAlongNormal / totalInv;
        sf::Vector2f impulse = info.normal * j;

        if (rbA && !rbA->isStatic) rbA->velocity -= impulse * invMassA;
        if (rbB && !rbB->isStatic) rbB->velocity += impulse * invMassB;

        // Zero-out la composante perpendiculaire si quasi au repos de A
        if (rbA && !rbA->isStatic)
        {
            float vnA = rbA->velocity.x * info.normal.x + rbA->velocity.y * info.normal.y;
            if (std::abs(vnA) < 0.05f)
            {
                rbA->velocity.x -= vnA * info.normal.x;
                rbA->velocity.y -= vnA * info.normal.y;
            }

        }
		// Zero-out la composante perpendiculaire si quasi au repos de B
        if (rbB && !rbB->isStatic)
        {
            float vnB = rbB->velocity.x * info.normal.x + rbB->velocity.y * info.normal.y;
            if (std::abs(vnB) < 0.05f)
            {
                rbB->velocity.x -= vnB * info.normal.x;
                rbB->velocity.y -= vnB * info.normal.y;
            }
        }
    }
};
