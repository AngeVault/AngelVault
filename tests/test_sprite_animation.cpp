#include "systems/sprite_animation_system.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

int main() {
    SpriteAnimator a;
    a.directions = 8;

    // ligne 0 à droite, puis sens horaire (y vers le bas)
    a.facing = { 1.f, 0.f };  CHECK(SpriteAnimationSystem::row(a) == 0);
    a.facing = { 1.f, 1.f };  CHECK(SpriteAnimationSystem::row(a) == 1);
    a.facing = { 0.f, 1.f };  CHECK(SpriteAnimationSystem::row(a) == 2);
    a.facing = { -1.f, 0.f }; CHECK(SpriteAnimationSystem::row(a) == 4);
    a.facing = { 0.f, -1.f }; CHECK(SpriteAnimationSystem::row(a) == 6);
    a.facing = { 1.f, -1.f }; CHECK(SpriteAnimationSystem::row(a) == 7);
    a.startAngle = 90.f;
    a.facing = { 0.f, 1.f };  CHECK(SpriteAnimationSystem::row(a) == 0);
    a.startAngle = 0.f;

    Registry r;
    Entity e = r.createEntity();
    r.addComponent<Geometry>(e, Geometry::SpriteData{}, 0);
    auto& anim = r.addComponent<SpriteAnimator>(e);
    anim.folder = "assets/body";
    anim.clip = "Walk";
    anim.frames = 15;
    anim.directions = 8;
    anim.fps = 10.f;
    anim.facing = { 0.f, 1.f };

    SpriteAnimationSystem sys;
    sys.onUpdate(r, 0.35f);   // image 3
    const auto& s = std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data);
    CHECK(s.textureId == "assets/body/Walk.png");
    CHECK(s.rect.position.x == 3 * 128 && s.rect.position.y == 2 * 128);
    CHECK(s.rect.size.x == 128 && s.rect.size.y == 128);

    sys.onUpdate(r, 1.25f);   // 16 images : bouclé sur 1
    CHECK(std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.position.x == 128);

    // changer de clip repart de l'image 0
    r.getComponent<SpriteAnimator>(e).clip = "Idle";
    sys.onUpdate(r, 0.01f);
    CHECK(std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.position.x == 0);
    CHECK(std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).textureId == "assets/body/Idle.png");

    if (g_failures == 0) std::printf("test_sprite_animation : OK\n");
    return g_failures == 0 ? 0 : 1;
}
