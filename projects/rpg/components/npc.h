#pragma once
#include "ecs/game_components.h"
#include <string>

// Composant de jeu « npc » (données seulement : la logique va dans un système ou un behavior).
// Après compilation : Inspector > Ajouter un composant > npc, ou glissez ce fichier sur une entité.
struct Npc {
    bool ennemies = false;
    float vision = 200.f;
};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(Npc, FIELD(ennemies), FIELD(vision))
