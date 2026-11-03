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
    float force = 0.f;
    float agilite = 0.f;
    float constitution = 0.f;
    float intelligence = 0.f;
    float sagesse = 0.f;
    float Volonte = 0.f;
    float charisme = 0.f;
};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(Stats, /*FIELD(classe),*/ FIELD(force), FIELD(agilite), FIELD(constitution), FIELD(intelligence), FIELD(sagesse), FIELD(Volonte), FIELD(charisme) )
