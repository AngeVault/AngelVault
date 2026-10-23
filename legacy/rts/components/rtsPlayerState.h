#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <ecs/entity.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ECS {
    // la 1ere ferme rapporte 3 or/s, chaque suivante 15% de moins que la precedente
    // avec un minimum pour que l'economie continue de grossir (et les grosses armees aussi)
    constexpr float kFarmBaseYield = 3.f;
    constexpr float kFarmDecay = 0.85f;
    constexpr float kFarmMinYield = 0.6f;

    // gain en or/s de la ferme numero rank (1 = la premiere)
    inline float farmYield(int rank) {
        return std::max(kFarmMinYield, kFarmBaseYield * std::pow(kFarmDecay, static_cast<float>(rank - 1)));
    }

    // gain total en or/s de toutes les fermes
    inline float farmIncomeTotal(int farms) {
        float total = 0.f;
        for (int rank = 1; rank <= farms; ++rank) total += farmYield(rank);
        return total;
    }

    struct PlayerEconomy {
        int gold = 500;

        // revenus des fermes (calcules par BuildingSystem, lus par le hud)
        int farmCount = 0;           // fermes terminees
        float incomePerSecond = 0.f; // or/s gagne grace aux fermes
        float incomeBuffer = 0.f;    // partie decimale pas encore ajoutee a l'or

        // salaires, chaque unite du joueur coute de l'or a chaque paie
        float wageInterval = 10.f;   // secondes entre deux paies
        float wageTimer = 10.f;      // temps restant avant la prochaine paie
        int wagePerCycle = 0;        // total a payer a la prochaine paie (pour le hud)
        bool unpaidUnits = false;    // true si on a pas pu tout payer
    };

    struct RtsScore {
        int value = 0;
    };

    struct RtsCommandQueue {
        enum class Type { BuildFarm, BuildBarracks, BuildTower, RecruitBuilder, RecruitWarrior, RecruitArcher, AssignBuilder };
        struct Command {
            Type type;
            Entity actor = NULL_ENTITY;
            sf::Vector2f position{ 0.f, 0.f };
            Entity target = NULL_ENTITY;
        };

        std::vector<Command> pending;
        std::string feedback;
        float feedbackTimer = 0.f;
    };

    struct RtsHudState {
        enum class PlacementMode { None, Farm, Barracks, Tower };
        enum class Action { BuildFarm, BuildBarracks, BuildTower, RecruitBuilder, RecruitWarrior, RecruitArcher, CancelPlacement };
        struct Button {
            sf::FloatRect bounds;
            std::string label;
            Action action;
        };

        PlacementMode placementMode = PlacementMode::None;
        bool gameOver = false;
        bool formationPreviewActive = false;
        sf::Vector2f formationPreviewCenter{ 0.f, 0.f };
        sf::Vector2f formationPreviewEnd{ 0.f, 0.f };
        uint32_t formationPreviewUnitCount = 0;
        std::vector<sf::Vector2f> formationPreviewPositions;
        std::array<std::vector<sf::Vector2f>, 4> savedFormationOffsets;
        std::array<std::vector<Entity>, 4> savedFormationUnits;
        int armedFormationShortcut = -1;
        std::vector<Button> buttons;
        std::string message;
        float messageTimer = 0.f;
    };
}
