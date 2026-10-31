#pragma once
#include <string>

class Registry;

struct ISystem {
    virtual ~ISystem() = default;
    virtual void onStart(Registry& registry) {}
    virtual void onUpdate(Registry& registry, float dt) = 0;
    virtual void onStop(Registry& registry) {}

    int priority = 0;

    // Activé / désactivé (éditeur : Exécuter > Systèmes..., ou le .angproj). Désactivé = onUpdate n'est plus appelé.
    bool enabled = true;
    // Nom affiché et enregistré (rempli par SystemManager::addSystem : le nom de la classe)
    std::string name;
};