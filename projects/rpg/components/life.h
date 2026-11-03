#pragma once
#include "ecs/game_components.h"
#include <string>

// Composant de jeu « life » (données seulement : la logique va dans un système ou un behavior).
// Après compilation : Inspector > Ajouter un composant > life, ou glissez ce fichier sur une entité.
struct Life {
    float actualLife = 1000.f;
    float maxLife = 1000.f;
    float regenlLife = 0.f;

};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(Life, FIELD(actualLife), FIELD(maxLife), FIELD(regenlLife))
