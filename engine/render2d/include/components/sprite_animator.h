#pragma once
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>
#include <string>

// anime le sprite de Geometry : une planche par clip (<folder>/<clip>.png), une ligne par direction
struct SpriteAnimator {
    std::string folder;                    // relatif au projet
    std::string clip = "Idle";
    sf::Vector2f frameSize{ 128.f, 128.f };
    int frames = 1;                        // colonnes de la planche
    int directions = 1;                    // lignes de la planche
    float startAngle = 0.f;                // angle de la ligne 0 en degrés (0 = droite), puis sens horaire
    float fps = 12.f;
    bool loop = true;
    sf::Vector2f facing{ 0.f, 1.f };       // choisit la ligne

    // état du jeu, pas sauvegardé
    std::string playing;
    float time = 0.f;
};

template<>
inline ReflectedComponent reflectComponent<SpriteAnimator>(SpriteAnimator& a) {
    return { "SpriteAnimator", {
        { "folder", PropertyInfo::Type::String, &a.folder },
        { "clip", PropertyInfo::Type::String, &a.clip },
        { "frameSize", PropertyInfo::Type::Vec2f, &a.frameSize },
        { "frames", PropertyInfo::Type::Int, &a.frames },
        { "directions", PropertyInfo::Type::Int, &a.directions },
        { "startAngle", PropertyInfo::Type::Float, &a.startAngle },
        { "fps", PropertyInfo::Type::Float, &a.fps },
        { "loop", PropertyInfo::Type::Bool, &a.loop },
        { "facing", PropertyInfo::Type::Vec2f, &a.facing },
    } };
}
