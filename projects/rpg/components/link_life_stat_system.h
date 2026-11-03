#pragma once
#include "game_systems.h"
#include "components/transform2d.h"
#include "../components/life.h"
#include "../components/stats.h"

// Système de jeu « linkLifeStat_system » : ajouté automatiquement au jeu après compilation.
// Activer / désactiver : éditeur > Exécuter > Systèmes...
class linkLifeStat_system : public ISystem {
public:
    linkLifeStat_system() { priority = 2; }   // ordre : physique 0, collisions 1, scripts 2, ... rendu 10
    // Pour dessiner : un constructeur linkLifeStat_system(sf::RenderWindow& window) est appelé à la place.

    void onStart(Registry& registry) override {
    }

    void onUpdate(Registry& registry, float dt) override {
        registry.view<Life, Stats>([&](Entity e, Life& life, Stats& stat) {
            switch (stat.classe)
            {
            case Stats::Class::MONSTRE:
                if (life.maxLife != stat.constitution)
                {
                    life.maxLife = stat.constitution;
                    life.regenlLife = stat.constitution / 100;
                }
                break;
            case Stats::Class::ARCHER:
                if (life.maxLife != stat.constitution)
                {
                    life.maxLife = stat.constitution;
                    life.regenlLife = stat.constitution / 100;
                }
                break;
            case Stats::Class::BARBARE:
                if (life.maxLife != stat.constitution)
                {
                    life.maxLife = stat.constitution;
                    life.regenlLife = stat.constitution / 100;
                }
                break;
            case Stats::Class::GERRIER:
                if (life.maxLife != stat.constitution)
                {
                    life.maxLife = stat.constitution;
                    life.regenlLife = stat.constitution / 100;
                }
                break;
            case Stats::Class::TANK:
                if (life.maxLife != stat.constitution)
                {
                    life.maxLife = stat.constitution;
                    life.regenlLife = stat.constitution / 100;
                }
                break;
            default: ;
                break;
            }
        });
    }
};
REGISTER_SYSTEM(linkLifeStat_system)
