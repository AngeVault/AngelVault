#pragma once

#include "../components/buildingComp.h"
#include "../components/rtsPlayerState.h"

// infos communes aux batiments constructibles (cout, correspondance placement / commande)
namespace Rts {

    inline int buildingCost(ECS::Building::Type type) {
        switch (type) {
        case ECS::Building::Type::Farm: return 150;
        case ECS::Building::Type::Barracks: return 250;
        case ECS::Building::Type::Tower: return 200;
        default: return 0;
        }
    }

    inline ECS::Building::Type buildingTypeForPlacement(ECS::RtsHudState::PlacementMode mode) {
        switch (mode) {
        case ECS::RtsHudState::PlacementMode::Barracks: return ECS::Building::Type::Barracks;
        case ECS::RtsHudState::PlacementMode::Tower: return ECS::Building::Type::Tower;
        default: return ECS::Building::Type::Farm;
        }
    }

    inline ECS::Building::Type buildingTypeForCommand(ECS::RtsCommandQueue::Type command) {
        switch (command) {
        case ECS::RtsCommandQueue::Type::BuildBarracks: return ECS::Building::Type::Barracks;
        case ECS::RtsCommandQueue::Type::BuildTower: return ECS::Building::Type::Tower;
        default: return ECS::Building::Type::Farm;
        }
    }

    inline ECS::RtsCommandQueue::Type buildCommandFor(ECS::Building::Type type) {
        switch (type) {
        case ECS::Building::Type::Barracks: return ECS::RtsCommandQueue::Type::BuildBarracks;
        case ECS::Building::Type::Tower: return ECS::RtsCommandQueue::Type::BuildTower;
        default: return ECS::RtsCommandQueue::Type::BuildFarm;
        }
    }

}
