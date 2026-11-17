#pragma once
#include "game_systems.h"
#include "components/transform2d.h"
// #include "../components/mon_composant.h"

// Système de jeu « ennemy_system » : ajouté automatiquement au jeu après compilation.
// Activer / désactiver : éditeur > Exécuter > Systèmes...
class ennemy_system : public ISystem {
public:
    ennemy_system() { priority = 3; }   // ordre : physique 0, collisions 1, scripts 2, ... rendu 10
    // Pour dessiner : un constructeur ennemy_system(sf::RenderWindow& window) est appelé à la place.

    void onStart(Registry& registry) override {
    }

    void onUpdate(Registry& registry, float dt) override {
        // registry.view<MonComposant, Transform2D>([&](Entity e, MonComposant& c, Transform2D& t) {
        // });
    }
};
REGISTER_SYSTEM(ennemy_system)
