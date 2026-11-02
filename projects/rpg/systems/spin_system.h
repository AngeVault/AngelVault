#pragma once
// Exemple de système de jeu : fait tourner toutes les entités qui ont Spin + Transform2D.
// Ajouté automatiquement au jeu ; à activer / désactiver dans l'éditeur (Exécuter > Systèmes...).
#include "game_systems.h"
#include "components/transform2d.h"
#include "../components/spin.h"

class SpinSystem : public ISystem {
public:
    SpinSystem() { priority = 3; }   // après la physique et les scripts, avant le rendu (10)

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Spin, Transform2D>([&](Entity, Spin& spin, Transform2D& t) {
            const float sign = spin.clockwise ? 1.f : -1.f;
            t.rotate(sf::degrees(sign * spin.degreesPerSecond * dt));
        });
    }
};
REGISTER_SYSTEM(SpinSystem)
