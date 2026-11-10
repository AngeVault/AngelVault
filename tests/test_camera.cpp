// Tests des systèmes caméra (sans fenêtre : pas de CameraSystem ni de CameraBoundsSystem)
#include "components/camera2d.h"
#include "components/camera_control.h"
#include "components/camera_follow.h"
#include "components/camera_shake.h"
#include "components/transform2d.h"
#include "systems/camera_control_system.h"
#include "systems/camera_follow_system.h"
#include "systems/camera_shake_system.h"
#include "systems/camera_system.h"
#include "utils/camera_utils.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)
static bool near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) < eps; }

static Entity makeCamera(Registry& r, int priority, sf::Vector2f pos = {}) {
    const Entity e = r.createEntity();
    r.addComponent<Transform2D>(e).setPosition(pos);
    r.addComponent<Camera2d>(e).priority = priority;
    return e;
}

int main() {
    // ---- Caméra affichée : la plus prioritaire parmi les actives
    {
        Registry r;
        const Entity a = makeCamera(r, 0);
        const Entity b = makeCamera(r, 5);
        CHECK(CameraSystem::activeCamera(r) == b);
        r.getComponent<Camera2d>(b).active = false;
        CHECK(CameraSystem::activeCamera(r) == a);
    }

    // ---- Suivi : calage au départ, ressort, zone morte, cible détruite
    {
        Registry r;
        const Entity cam = makeCamera(r, 0);
        const Entity player = r.createEntity();
        r.addComponent<Transform2D>(player).setPosition({ 100.f, 50.f });
        Camera::follow(r, cam, player);
        r.getComponent<CameraFollow>(cam).offset = { 0.f, -10.f };
        CameraFollowSystem follow;

        follow.onUpdate(r, 0.016f);
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().x, 100.f));   // calée sur la cible
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().y, 40.f));

        r.getComponent<Transform2D>(player).setPosition({ 300.f, 50.f });
        follow.onUpdate(r, 0.016f);
        const float x1 = r.getComponent<Transform2D>(cam).getPosition().x;
        CHECK(x1 > 100.f && x1 < 300.f);                                         // lissée
        for (int i = 0; i < 200; ++i) follow.onUpdate(r, 0.016f);
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().x, 300.f, 0.5f));

        auto& f = r.getComponent<CameraFollow>(cam);
        f.deadZone = { 50.f, 50.f };
        f.smoothTime = 0.f;
        r.getComponent<Transform2D>(player).setPosition({ 330.f, 50.f });        // reste dans la zone morte
        follow.onUpdate(r, 0.016f);
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().x, 300.f, 0.5f));
        r.getComponent<Transform2D>(player).setPosition({ 400.f, 50.f });        // en sort de 50
        follow.onUpdate(r, 0.016f);
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().x, 350.f, 0.5f));

        r.destroyEntity(player);
        r.flushPendingDestroys();
        follow.onUpdate(r, 0.016f);
        CHECK(r.getComponent<CameraFollow>(cam).target == NULL_ENTITY);          // redevient libre
    }

    // ---- Tremblement : décalage borné, trauma qui retombe, effets séparés du Transform2D
    {
        Registry r;
        const Entity cam = makeCamera(r, 0, { 10.f, 20.f });
        Camera::shake(r, 0.8f);
        CHECK(r.hasComponent<CameraShake>(cam) && near(r.getComponent<CameraShake>(cam).trauma, 0.8f));
        Camera::shake(r, 0.8f);
        CHECK(near(r.getComponent<CameraShake>(cam).trauma, 1.f));               // plafonné à 1

        CameraShakeSystem shake;
        bool moved = false, bounded = true;
        for (int i = 0; i < 10; ++i) {
            auto& c = r.getComponent<Camera2d>(cam);
            c.effectOffset = {};
            shake.onUpdate(r, 0.016f);
            moved |= c.effectOffset.x != 0.f || c.effectOffset.y != 0.f;
            bounded &= std::abs(c.effectOffset.x) <= 24.f && std::abs(c.effectOffset.y) <= 24.f;
        }
        CHECK(moved && bounded);
        CHECK(near(r.getComponent<Transform2D>(cam).getPosition().x, 10.f));     // pas touché
        for (int i = 0; i < 200; ++i) shake.onUpdate(r, 0.016f);
        CHECK(r.getComponent<CameraShake>(cam).trauma == 0.f);

        for (int i = 0; i < 1000; ++i) {
            const float n = CameraShakeSystem::noise(i * 0.137f, 3);
            CHECK(n >= -1.f && n <= 1.f);
        }
    }

    // ---- Contrôle : zoom visé lissé et borné (sans Game : les entrées valent 0)
    {
        Registry r;
        const Entity cam = makeCamera(r, 0);
        auto& control = r.addComponent<CameraControl>(cam);
        control.targetZoom = 10.f;   // au-delà de maxZoom (4)
        CameraControlSystem sys;
        for (int i = 0; i < 300; ++i) sys.onUpdate(r, 0.016f);
        CHECK(near(r.getComponent<CameraControl>(cam).targetZoom, 4.f));
        CHECK(near(r.getComponent<Camera2d>(cam).zoom, 4.f));
    }

    if (g_failures == 0) std::printf("test_camera : OK\n");
    return g_failures == 0 ? 0 : 1;
}
