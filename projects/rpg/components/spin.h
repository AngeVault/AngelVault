#pragma once
// Exemple de composant de jeu (déclaré dans le projet, pas dans le moteur).
// Inspector > Ajouter un composant > Spin, ou glisser ce fichier sur une entité.
// C'est SpinSystem (systems/spin_system.h) qui fait tourner les entités qui l'ont.
#include "ecs/game_components.h"

struct Spin {
    float degreesPerSecond = 90.f;   // vitesse de rotation
    bool clockwise = true;
};
REGISTER_COMPONENT(Spin, FIELD(degreesPerSecond), FIELD(clockwise))
