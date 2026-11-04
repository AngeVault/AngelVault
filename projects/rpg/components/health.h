#pragma once
#include "ecs/game_components.h"
#include <SFML/System/Vector2.hpp>
#include <string>

// Composant de jeu « health » (données seulement : la logique va dans un système ou un behavior).
// Après compilation : Inspector > Ajouter un composant > health, ou glissez ce fichier sur une entité.
struct health {
    float value = 1.f;
    // int count = 0;   bool active = true;   sf::Vector2f direction{ 1.f, 0.f };   std::string tag;
};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(health, FIELD(value))
