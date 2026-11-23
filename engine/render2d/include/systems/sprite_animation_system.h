#pragma once
#include "components/geometry.h"
#include "components/sprite_animator.h"
#include "ecs/registry.h"
#include "ecs/system.h"

#include <algorithm>
#include <cmath>
#include <numbers>

// choisit la case de la planche : colonne = temps, ligne = direction regardée
class SpriteAnimationSystem : public ISystem {
public:
    SpriteAnimationSystem() { priority = 9; }   // avant le rendu

    void onUpdate(Registry& registry, float dt) override {
        registry.view<SpriteAnimator, Geometry>([&](Entity, SpriteAnimator& a, Geometry& g) {
            auto* sprite = std::get_if<Geometry::SpriteData>(&g.data);
            if (!sprite || a.clip.empty()) return;
            if (a.clip != a.playing) { a.playing = a.clip; a.time = 0.f; }
            a.time += dt * a.fps;
            apply(a, *sprite);
        });
    }

    static int row(const SpriteAnimator& a) {
        if (a.directions <= 1 || (a.facing.x == 0.f && a.facing.y == 0.f)) return 0;
        const float deg = std::atan2(a.facing.y, a.facing.x) * 180.f / std::numbers::pi_v<float> - a.startAngle;
        const int r = int(std::lround(deg / (360.f / a.directions))) % a.directions;
        return r < 0 ? r + a.directions : r;
    }

    static void apply(const SpriteAnimator& a, Geometry::SpriteData& sprite) {
        const int frames = std::max(1, a.frames);
        const int t = std::max(0, int(a.time));
        const int column = a.loop ? t % frames : std::min(t, frames - 1);
        const std::string texture = a.folder.empty() ? a.clip + ".png" : a.folder + "/" + a.clip + ".png";
        if (sprite.textureId != texture) sprite.textureId = texture;
        const sf::Vector2i size{ int(a.frameSize.x), int(a.frameSize.y) };
        sprite.rect = { { column * size.x, row(a) * size.y }, size };
    }
};
