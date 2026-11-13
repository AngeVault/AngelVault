#pragma once

#include "ecs/registry.h"
#include "ecs/system.h"
#include "components/collider2d.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"
#include "tilemap/tilemap.h"

#include <array>
#include <cmath>
#include <vector>

struct TileCollider {
    Entity map = NULL_ENTITY;
};

class TileCollisionSystem : public ISystem {
public:
    TileCollisionSystem() { priority = 0; }  

    void onUpdate(Registry& registry, float) override {
        for (Built& b : m_built) b.seen = false;

        registry.view<Tilemap, Transform2D>([&](Entity e, Tilemap& tm, Transform2D& t) {
            Built* b = find(e);
            if (!b) b = &m_built.emplace_back(Built{ e });
            b->seen = true;
            const TileMapData* data = tm.data.get();
            const uint32_t version = data ? data->version : 0;
            if (b->data == data && b->version == version && b->matrix == t.getLocalMatrix()) return;

            clear(registry, *b);
            b->data = data;
            b->version = version;
            b->matrix = t.getLocalMatrix();
            if (data) build(registry, *b, *data);
        });

        for (std::size_t i = 0; i < m_built.size();) {
            if (m_built[i].seen) { ++i; continue; }
            clear(registry, m_built[i]);
            m_built.erase(m_built.begin() + std::ptrdiff_t(i));
        }
    }

    void onStop(Registry& registry) override {
        for (Built& b : m_built) clear(registry, b);
        m_built.clear();
    }

private:
    struct Built {
        Entity map;
        const TileMapData* data = nullptr;
        uint32_t version = 0;
        sf::Transform matrix;
        std::vector<Entity> colliders;
        bool seen = false;
    };
    std::vector<Built> m_built;

    Built* find(Entity map) {
        for (Built& b : m_built) if (b.map == map) return &b;
        return nullptr;
    }

    static void clear(Registry& registry, Built& b) {
        for (Entity c : b.colliders) registry.destroyEntity(c);
        b.colliders.clear();
    }

    static void build(Registry& registry, Built& b, const TileMapData& data) {
        for (const auto& area : data.solidAreas()) {
            std::array<sf::Vector2f, 4> world;
            sf::Vector2f center;
            for (std::size_t i = 0; i < 4; ++i) {
                world[i] = b.matrix.transformPoint(area[i]);
                center += world[i] * 0.25f;
            }

            Collider2D col;
            const bool axisAligned = world[0].y == world[1].y && world[1].x == world[2].x;
            if (axisAligned) {
                col.shape = Collider2D::Box{ { std::abs(world[1].x - world[0].x) * 0.5f, std::abs(world[2].y - world[1].y) * 0.5f } };
            } else {
                Collider2D::Polygon poly;
                for (sf::Vector2f p : world) poly.points.push_back(p - center);
                col.shape = std::move(poly);
            }

            RigidBody2D body;
            body.isStatic = true;
            body.useGravity = false;

            const Entity c = registry.createEntity();
            registry.addComponent<Transform2D>(c).setPosition(center);
            registry.addComponent<Collider2D>(c, std::move(col));
            registry.addComponent<RigidBody2D>(c, body);
            registry.addComponent<TileCollider>(c, TileCollider{ b.map });
            b.colliders.push_back(c);
        }
    }
};
