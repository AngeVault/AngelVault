#pragma once
// Garde la vue des caméras CameraBounds dans leur zone (zoom mini + position).
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/camera2d.h"
#include "components/camera_bounds.h"
#include "components/camera_control.h"
#include "components/transform2d.h"
#include "systems/camera_system.h"

#include <algorithm>

class CameraBoundsSystem : public ISystem {
public:
    explicit CameraBoundsSystem(sf::RenderTarget& target) : m_target(target) { priority = 5; }

    void onUpdate(Registry& registry, float) override {
        const sf::Vector2u screen = CameraSystem::screenSize(m_target);
        if (screen.x == 0 || screen.y == 0) return;
        registry.view<Camera2d, CameraBounds, Transform2D>([&](Entity e, Camera2d& cam, CameraBounds& b, Transform2D& t) {
            if (!b.valid()) return;
            const sf::Vector2f world = b.max - b.min;

            // Zoom mini : la vue ne dépasse pas la zone
            if (b.limitZoom) {
                const float fit = std::min(std::max(screen.x / world.x, screen.y / world.y), cam.maxZoom);
                cam.zoom = std::max(cam.zoom, fit);
                if (registry.hasComponent<CameraControl>(e)) {   // sinon le zoom visé reste « coincé » dessous
                    float& wanted = registry.getComponent<CameraControl>(e).targetZoom;
                    if (wanted > 0.f) wanted = std::max(wanted, fit);
                }
            }

            const sf::Vector2f half = CameraSystem::visibleSize(cam.zoom, screen) * 0.5f;
            sf::Vector2f p = t.getPosition();
            p.x = clampAxis(p.x, b.min.x + half.x, b.max.x - half.x);
            p.y = clampAxis(p.y, b.min.y + half.y, b.max.y - half.y);
            if (p != t.getPosition()) t.setPosition(p);
        });
    }

private:
    static float clampAxis(float v, float lo, float hi) {
        return lo > hi ? (lo + hi) * 0.5f : std::clamp(v, lo, hi);   // zone plus petite que la vue : centrée
    }
    sf::RenderTarget& m_target;
};
