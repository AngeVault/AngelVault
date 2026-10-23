#pragma once
#include <SFML/Graphics.hpp>
#include "unitComp.h"

// Teinte par type + équipe (temporaire, en attendant les vraies textures)
inline sf::Color unitTint(ECS::Unit::Type type, ECS::Unit::Team team) {
    if (team == ECS::Unit::Team::Enemy)
        return sf::Color(150, 150, 150); // gris = dominé

    switch (type) {
    case ECS::Unit::Type::Melee:  return sf::Color(150, 180, 255); // bleu
    case ECS::Unit::Type::Archer: return sf::Color(255, 220, 120); // jaune
    case ECS::Unit::Type::Builder: return sf::Color(210, 170, 115); // brun clair
    case ECS::Unit::Type::Healer: return sf::Color(120, 255, 170); // vert
    default:                      return sf::Color::White;         // blanc
    }
}

// Identifiant de texture par type + équipe (pour plus tard)
inline std::string unitTexture(ECS::Unit::Type type, ECS::Unit::Team team) {
    if (team == ECS::Unit::Team::Enemy) {
        switch (type) {
        case ECS::Unit::Type::Melee:  return "enemy_melee";
        case ECS::Unit::Type::Archer: return "enemy_archer";
        case ECS::Unit::Type::Healer: return "enemy_healer";
        default:                      return "enemy_soldier";
        }
    }
    switch (type) {
    case ECS::Unit::Type::Melee:  return "player_melee";
    case ECS::Unit::Type::Archer: return "player_archer";
    case ECS::Unit::Type::Healer: return "player_healer";
    default:                      return "player";
    }
}
