#pragma once
#include "ecs/game_components.h"
#include <string>

// Composant de jeu « character » (données seulement : la logique va dans un système ou un behavior).
// Après compilation : Inspector > Ajouter un composant > character, ou glissez ce fichier sur une entité.

struct Stats {

    enum class Class
    {
        MONSTRE,
        ARCHER,
        BARBARE,
        GERRIER,
        TANK
    };
    enum Class classe = Stats::Class::MONSTRE;
    float force = 1.f;
    float agilite = 1.f;
    float constitution = 1.f;
    float intelligence = 1.f;
    float sagesse = 1.f;
    float Volonte = 1.f;
    float charisme = 1.f;
};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(Stats, /*FIELD(classe),*/ FIELD(force), FIELD(agilite), FIELD(constitution), FIELD(intelligence), FIELD(sagesse), FIELD(Volonte), FIELD(charisme) )
