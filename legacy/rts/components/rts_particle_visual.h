#pragma once

#include <SFML/Graphics/Color.hpp>

// Configuration purement visuelle de l'effet RTS, à placer sur l'entité émettrice.
// Le moteur ne connaît pas ce composant : il appartient au jeu et à son rendu.
struct RtsParticleVisual {
    sf::Color startColor = sf::Color(255, 225, 240, 230);
    sf::Color endColor = sf::Color(255, 120, 185, 0);
    float startRadius = 5.f;
    float endRadius = 1.f;
    int segments = 10;
    float stretch = 0.f; // > 0 : etincelle allongee dans le sens de la vitesse (secondes de trajet)
    float glow = 0.f;    // > 0 : halo doux autour (0.8 = halo 1.8x plus grand)
    bool soft = false;   // bord transparent (fumee, flash), sinon disque plein
};
