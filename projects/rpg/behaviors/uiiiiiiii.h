#pragma once
#include "ecs/behavior.h"
#include "ecs/behavior_factory.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "input/input.h"

// Comportement « uiiiiiiii ».
// Après compilation, glissez ce fichier sur une entité (Hierarchy, Inspector ou vue scène),
// ou Inspector > Ajouter un composant > Comportement > uiiiiiiii.
class uiiiiiiii : public IBehavior {
public:
    // Réglages visibles dans l'Inspector et enregistrés dans la scène (float, int, bool, sf::Vector2f, std::string)
    PROPERTY(float, speed, 100.f)

    // Appelé une fois, avant le premier onUpdate
    void onStart(Entity self, Registry& registry) override {
    }

    // Appelé à chaque frame (dt en secondes)
    void onUpdate(Entity self, Registry& registry, float dt) override {
        auto& transform = registry.getComponent<Transform2D>(self);
        transform.move({ Input::getAxis("Horizontal") * speed * dt, 0.f });
    }
};

// Nom utilisé dans les fichiers .scene / .prefab
REGISTER_BEHAVIOR(uiiiiiiii)
