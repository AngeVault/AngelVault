#pragma once
#include "ecs/behavior.h"
#include "ecs/behavior_factory.h"
#include "ecs/registry.h"
#include "input/input.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include "components/collider2d.h"
#include "components/transform2d.h"
#include "../components/life.h"
#include "../components/npc.h"

// Comportement « monster_behaviors ».
// Après compilation, glissez ce fichier sur une entité (Hierarchy, Inspector ou vue scène),
// ou Inspector > Ajouter un composant > Comportement > monster_behaviors.
class monster_behaviors : public IBehavior {
public:
    // Réglages visibles dans l'Inspector et enregistrés dans la scène (float, int, bool, sf::Vector2f, std::string)
    PROPERTY(float, speed, 100.f)

    // Appelé une fois, avant le premier onUpdate
    void onStart(Entity self, Registry& registry) override {
    }

    // Appelé à chaque frame (dt en secondes)
    void onUpdate(Entity self, Registry& registry, float dt) override {
        auto& life = registry.getComponent<Life>(self);
        if (life.actualLife > 0)
        {
            auto& npc = registry.getComponent<Npc>(self);
            if (npc.ennemies)
            {
                
            }
        }
    }
};

// Nom utilisé dans les fichiers .scene / .prefab
REGISTER_BEHAVIOR(monster_behaviors)
