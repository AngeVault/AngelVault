#pragma once

#include "../components/buildingComp.h"
#include "../components/rtsPlayerState.h"
#include "../components/unitComp.h"
#include "../utils/entity_factory.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <algorithm>
#include <cmath>
#include <vector>

class BuildingSystem : public ISystem {
public:
    BuildingSystem() { priority = -8; }

    void onUpdate(Registry& registry, float dt) override {
        bool gameOver = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            gameOver = hud.gameOver;
        });
        if (gameOver) return;

        struct PendingRecruit { ECS::Unit::Type type; sf::Vector2f position; };
        std::vector<PendingRecruit> pendingRecruits;
        ECS::PlayerEconomy* economy = nullptr;
        registry.view<ECS::PlayerEconomy>([&](Entity, ECS::PlayerEconomy& value) { economy = &value; });
        if (!economy) return;

        // les fermes en construction ne rapportent rien
        int finishedFarms = 0;
        registry.view<ECS::Building>([&](Entity, ECS::Building& building) {
            if (building.type == ECS::Building::Type::Farm && building.pv > 0
                && building.constructionProgress >= 1.f)
                ++finishedFarms;
        });
        economy->farmCount = finishedFarms;
        economy->incomePerSecond = ECS::farmIncomeTotal(finishedFarms);
        economy->incomeBuffer += economy->incomePerSecond * dt;
        const int earned = static_cast<int>(economy->incomeBuffer);
        economy->gold += earned;
        economy->incomeBuffer -= static_cast<float>(earned);

        registry.view<ECS::Building, Transform2D>([&](Entity buildingEntity,
            ECS::Building& building, Transform2D& transform) {
            if (building.pv <= 0) return;
            if (building.constructionProgress < 1.f) {
                int buildersAtSite = 0;
                const sf::Vector2f site = transform.getPosition();
                registry.view<ECS::Unit, Transform2D>([&](Entity, ECS::Unit& unit, Transform2D& builderTransform) {
                    if (unit.type != ECS::Unit::Type::Builder || unit.team != ECS::Unit::Team::Player
                        || unit.constructionTarget != buildingEntity) return;
                    const sf::Vector2f position = builderTransform.getPosition();
                    if (std::hypot(position.x - site.x, position.y - site.y) <= std::max(80.f, building.attackRadius + 24.f))
                        ++buildersAtSite;
                });
                building.assignedBuilderCount = buildersAtSite;
                const float duration = std::max(building.constructionDuration, 0.01f);
                if (buildersAtSite > 0)
                    building.constructionProgress = std::min(1.f,
                        building.constructionProgress + dt * buildersAtSite / duration);
            }
            const float visualScale = 0.08f + 0.92f * building.constructionProgress;
            transform.setScale({ visualScale, visualScale });

            if (building.constructionProgress >= 1.f && building.assignedBuilderCount > 0) {
                registry.view<ECS::Unit>([&](Entity, ECS::Unit& unit) {
                    if (unit.constructionTarget == buildingEntity)
                        unit.constructionTarget = NULL_ENTITY;
                });
                building.assignedBuilderCount = 0;
            }

            if (building.constructionProgress >= 1.f && !building.recruitmentQueue.empty()) {
                building.recruitmentProgress += dt;
                while (!building.recruitmentQueue.empty()) {
                    const ECS::Building::RecruitmentType next = building.recruitmentQueue.front();
                    const float duration = next == ECS::Building::RecruitmentType::Archer ? 7.f : 5.f;
                    if (building.recruitmentProgress < duration) break;
                    building.recruitmentProgress -= duration;
                    building.recruitmentQueue.pop_front();
                    const ECS::Unit::Type unitType = next == ECS::Building::RecruitmentType::Archer
                        ? ECS::Unit::Type::Archer
                        : (next == ECS::Building::RecruitmentType::Builder
                            ? ECS::Unit::Type::Builder : ECS::Unit::Type::Soldier);
                    pendingRecruits.push_back({ unitType,
                        transform.getPosition() + sf::Vector2f(building.halfSize.x + 25.f, 0.f) });
                }
                if (building.recruitmentQueue.empty()) building.recruitmentProgress = 0.f;
            }
        });
        for (const PendingRecruit& recruit : pendingRecruits)
            Rts::createUnit(registry, recruit.type, ECS::Unit::Team::Player, recruit.position);
    }
};
