#pragma once

#include "../components/buildingComp.h"
#include "../components/rtsPlayerState.h"
#include "../components/selectable.h"
#include "../components/unitComp.h"
#include "../RtsTile_map.hpp"
#include "../utils/entity_factory.h"
#include "../utils/building_placement.h"
#include "../utils/building_info.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <algorithm>
#include <cmath>

class RtsCommandSystem : public ISystem {
public:
    explicit RtsCommandSystem(TileMap& tileMap) : m_tileMap(tileMap) { priority = -9; }

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) return;

        registry.view<ECS::RtsCommandQueue, ECS::PlayerEconomy>(
            [&](Entity, ECS::RtsCommandQueue& queue, ECS::PlayerEconomy& economy) {
                if (queue.feedbackTimer > 0.f)
                    queue.feedbackTimer = std::max(0.f, queue.feedbackTimer - dt);
                if (!queue.pending.empty())
                    queue.feedbackTimer = 2.5f;
                for (const ECS::RtsCommandQueue::Command& command : queue.pending)
                    process(registry, queue, economy, command);
                queue.pending.clear();
            });
    }

private:
    TileMap& m_tileMap;

    void process(Registry& registry, ECS::RtsCommandQueue& queue,
        ECS::PlayerEconomy& economy, const ECS::RtsCommandQueue::Command& command) {
        switch (command.type) {
        case ECS::RtsCommandQueue::Type::BuildFarm:
        case ECS::RtsCommandQueue::Type::BuildBarracks:
        case ECS::RtsCommandQueue::Type::BuildTower:
            construct(registry, queue, economy, command);
            break;
        case ECS::RtsCommandQueue::Type::RecruitBuilder:
        case ECS::RtsCommandQueue::Type::RecruitWarrior:
        case ECS::RtsCommandQueue::Type::RecruitArcher:
            recruit(registry, queue, economy, command);
            break;
        case ECS::RtsCommandQueue::Type::AssignBuilder:
            assignBuilder(registry, queue, command);
            break;
        }
    }

    void construct(Registry& registry, ECS::RtsCommandQueue& queue,
        ECS::PlayerEconomy& economy, const ECS::RtsCommandQueue::Command& command) {
        if (!registry.isValid(command.actor)
            || !registry.hasComponent<ECS::Unit>(command.actor)
            || !registry.hasComponent<ECS::Selectable>(command.actor)
            || !registry.hasComponent<Transform2D>(command.actor)) {
            queue.feedback = "Selectionne un batisseur pour construire.";
            return;
        }
        const ECS::Unit& builder = registry.getComponent<ECS::Unit>(command.actor);
        const ECS::Selectable& selection = registry.getComponent<ECS::Selectable>(command.actor);
        if (builder.type != ECS::Unit::Type::Builder || !selection.playerOwned || !selection.selected) {
            queue.feedback = "Selectionne un batisseur pour construire.";
            return;
        }

        const ECS::Building::Type buildingType = Rts::buildingTypeForCommand(command.type);
        const int cost = Rts::buildingCost(buildingType);
        if (economy.gold < cost) {
            queue.feedback = "Pas assez d'or pour construire.";
            return;
        }

        const Rts::BuildingFootprint footprint = Rts::buildingFootprint(buildingType);
        const sf::Vector2f topLeft = command.position - sf::Vector2f(
            footprint.tiles.x * static_cast<float>(m_tileMap.tileSize) * 0.5f,
            footprint.tiles.y * static_cast<float>(m_tileMap.tileSize) * 0.5f);
        const sf::Vector2i tile = m_tileMap.worldToTile(topLeft);
        if (!Rts::isValidBuildingPlacement(registry, m_tileMap, buildingType, tile)) {
            queue.feedback = "Choisis une zone rose libre sur la grille.";
            return;
        }

        economy.gold -= cost;
        const Entity buildingEntity = Rts::createBuilding(registry,
            buildingType, Rts::buildingCenterForTile(m_tileMap, tile, buildingType));
        ECS::Unit& builderUnit = registry.getComponent<ECS::Unit>(command.actor);
        builderUnit.constructionTarget = buildingEntity;
        builderUnit.target = NULL_ENTITY;
        builderUnit.hasDestination = false;
        builderUnit.movementGroup = 0;
        queue.feedback = "Construction lancee ("
            + std::to_string(static_cast<int>(registry.getComponent<ECS::Building>(buildingEntity).constructionDuration))
            + " s).";
    }

    void assignBuilder(Registry& registry, ECS::RtsCommandQueue& queue,
        const ECS::RtsCommandQueue::Command& command) {
        if (!registry.isValid(command.actor) || !registry.hasComponent<ECS::Building>(command.actor)
            || !registry.hasComponent<ECS::Selectable>(command.actor)
            || !registry.hasComponent<ECS::Unit>(command.target)) return;
        const ECS::Building& building = registry.getComponent<ECS::Building>(command.actor);
        ECS::Unit& builder = registry.getComponent<ECS::Unit>(command.target);
        if (building.pv <= 0 || building.constructionProgress >= 1.f || building.type == ECS::Building::Type::Headquarters
            || builder.type != ECS::Unit::Type::Builder || builder.team != ECS::Unit::Team::Player
            || !registry.getComponent<ECS::Selectable>(command.actor).playerOwned) {
            queue.feedback = "Ce batiment ne peut pas recevoir de batisseur.";
            return;
        }
        builder.constructionTarget = command.actor;
        builder.target = NULL_ENTITY;
        builder.hasDestination = false;
        builder.movementGroup = 0;
        queue.feedback = "Batisseur assigne au chantier.";
    }

    void recruit(Registry& registry, ECS::RtsCommandQueue& queue,
        ECS::PlayerEconomy& economy, const ECS::RtsCommandQueue::Command& command) {
        if (!registry.isValid(command.actor)
            || !registry.hasComponent<ECS::Building>(command.actor)
            || !registry.hasComponent<ECS::Selectable>(command.actor)
            || !registry.hasComponent<Transform2D>(command.actor)) {
            queue.feedback = "Selectionne un batiment de production.";
            return;
        }
        ECS::Building& building = registry.getComponent<ECS::Building>(command.actor);
        const ECS::Selectable& selection = registry.getComponent<ECS::Selectable>(command.actor);
        if (building.pv <= 0 || !selection.playerOwned || !selection.selected) {
            queue.feedback = "Selectionne un batiment de production.";
            return;
        }
        if (building.constructionProgress < 1.f) {
            queue.feedback = "Ce batiment est encore en construction.";
            return;
        }

        ECS::Unit::Type unitType;
        int cost;
        if (command.type == ECS::RtsCommandQueue::Type::RecruitBuilder
            && building.type == ECS::Building::Type::Headquarters) {
            unitType = ECS::Unit::Type::Builder;
            cost = 100;
        }
        else if (command.type == ECS::RtsCommandQueue::Type::RecruitWarrior
            && building.type == ECS::Building::Type::Barracks) {
            unitType = ECS::Unit::Type::Soldier;
            cost = 100;
        }
        else if (command.type == ECS::RtsCommandQueue::Type::RecruitArcher
            && building.type == ECS::Building::Type::Barracks) {
            unitType = ECS::Unit::Type::Archer;
            cost = 150;
        }
        else return;

        if (economy.gold < cost) {
            queue.feedback = "Pas assez d'or.";
            return;
        }
        economy.gold -= cost;
        const ECS::Building::RecruitmentType recruitmentType = unitType == ECS::Unit::Type::Builder
            ? ECS::Building::RecruitmentType::Builder
            : (unitType == ECS::Unit::Type::Archer
                ? ECS::Building::RecruitmentType::Archer
                : ECS::Building::RecruitmentType::Soldier);
        building.recruitmentQueue.push_back(recruitmentType);
        queue.feedback = unitType == ECS::Unit::Type::Archer
            ? "Archer en formation (7 s)."
            : (unitType == ECS::Unit::Type::Builder
                ? "Batisseur en formation (5 s)."
                : "Soldat en formation (5 s).");
    }
};
