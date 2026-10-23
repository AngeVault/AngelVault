#pragma once

#include "../components/rtsPlayerState.h"
#include "../components/unitComp.h"
#include <ecs/registry.h>
#include <ecs/system.h>
#include <string>

// salaires : toutes les wageInterval secondes chaque unite du joueur coute de l'or (convertis inclus)
// si y a pas assez d'or les unites non payees font greve (degats /2) jusqu'a la prochaine paie
class WageSystem : public ISystem {
public:
    WageSystem() { priority = -7; }

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        ECS::RtsHudState* hud = nullptr;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& value) {
            hud = &value;
            gameOver = value.gameOver;
        });
        if (gameOver) return;

        registry.view<ECS::PlayerEconomy>([&](Entity, ECS::PlayerEconomy& economy) {
            int due = 0;
            registry.view<ECS::Unit>([&](Entity, ECS::Unit& unit) {
                if (unit.team == ECS::Unit::Team::Player && unit.pv > 0)
                    due += ECS::unitWage(unit.type);
            });
            economy.wagePerCycle = due;

            economy.wageTimer -= dt;
            if (economy.wageTimer > 0.f) return;
            economy.wageTimer += economy.wageInterval;

            bool someoneUnpaid = false;
            registry.view<ECS::Unit>([&](Entity, ECS::Unit& unit) {
                if (unit.team != ECS::Unit::Team::Player || unit.pv <= 0) {
                    unit.unpaid = false;
                    return;
                }
                const int wage = ECS::unitWage(unit.type);
                if (economy.gold >= wage) {
                    economy.gold -= wage;
                    unit.unpaid = false;
                }
                else {
                    unit.unpaid = true;
                    someoneUnpaid = true;
                }
            });
            economy.unpaidUnits = someoneUnpaid;

            if (hud && someoneUnpaid) {
                hud->message = "Salaires impayes : des unites font greve !";
                hud->messageTimer = 3.f;
            }
        });
    }
};
