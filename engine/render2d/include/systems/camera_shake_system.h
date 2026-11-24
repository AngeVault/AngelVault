#pragma once
// Tremblement (CameraShake) : ajoute un décalage / une rotation à la vue de la frame, puis le trauma retombe.
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/camera2d.h"
#include "components/camera_shake.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class CameraShakeSystem : public ISystem {
public:
    CameraShakeSystem() { priority = 6; }

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Camera2d, CameraShake>([&](Entity, Camera2d& cam, CameraShake& s) {
            if (s.trauma <= 0.f) { s.trauma = 0.f; return; }
            s.time += dt;
            const float amount = std::pow(s.trauma, s.exponent);
            const float t = s.time * s.frequency;
            cam.effectOffset += sf::Vector2f(noise(t, 0) * s.maxOffset.x * amount, noise(t, 1) * s.maxOffset.y * amount);
            cam.effectRotation += noise(t, 2) * s.maxRotation * amount;
            s.trauma = std::max(0.f, s.trauma - s.decay * dt);
        });
    }

    // Bruit lissé dans [-1, 1], un canal par `seed`
    static float noise(float t, uint32_t seed) {
        const float i = std::floor(t);
        const float f = t - i;
        const float u = f * f * (3.f - 2.f * f);
        const auto n = static_cast<int32_t>(i);
        return lattice(n, seed) + (lattice(n + 1, seed) - lattice(n, seed)) * u;
    }

private:
    static float lattice(int32_t i, uint32_t seed) {
        uint32_t h = static_cast<uint32_t>(i) * 374761393u + seed * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF) * 2.f - 1.f;
    }
};
