#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/day_night_cycle.h"

#include <SFML/Graphics.hpp>

#include <optional>

// Avance l'heure du DayNightCycle et teinte l'écran : un rectangle plein écran multiplié avec l'image
// (BlendMultiply), passé dans shaders/daynight.frag pour le vignettage. Sans shader (fichier absent, vieille carte) :
// la teinte seule. Rien n'est dessiné si la scène n'a pas de DayNightCycle.
class DayNightSystem : public ISystem {
public:
    explicit DayNightSystem(sf::RenderTarget& target);   // le shader est chargé au premier dessin
    void onUpdate(Registry& registry, float dt) override;

private:
    void loadShader();

    sf::RenderTarget& m_target;
    std::optional<sf::Shader> m_shader;
    bool m_shaderTried = false;
};
