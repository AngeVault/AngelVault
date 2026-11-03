#pragma once
#include "game_systems.h"
#include "../components/life.h"

// Système de jeu « life_system » : ajouté automatiquement au jeu après compilation.
// Activer / désactiver : éditeur > Exécuter > Systèmes...
class life_system : public ISystem {
public:
    life_system() { priority = 3; }   // ordre : physique 0, collisions 1, scripts 2, ... rendu 10
    // Pour dessiner : un constructeur life_system(sf::RenderWindow& window) est appelé à la place.

    void onStart(Registry& registry) override {
    }

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Life>([&](Entity e, Life& life)
        {
            if (life.actualLife > 0)
            {
                if (life.actualLife < life.maxLife)
                {
                    life.actualLife += life.regenlLife*dt;
                    if (life.actualLife > life.maxLife) life.actualLife = life.maxLife;
                }
            }

        });
    }   
};
REGISTER_SYSTEM(life_system)
