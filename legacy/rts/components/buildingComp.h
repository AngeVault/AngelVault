#pragma once
#include <SFML/System/Vector2.hpp>
#include <deque>

namespace ECS {
    struct Building {
        enum class Type { Headquarters, Farm, Barracks, Tower };
        enum class RecruitmentType { Builder, Soldier, Archer };

        Type type = Type::Farm;
        float constructionProgress = 1.f;
        float constructionDuration = 4.f;
        int assignedBuilderCount = 0;
        std::deque<RecruitmentType> recruitmentQueue;
        float recruitmentProgress = 0.f;
        int pv = 500;
        int pvMax = 500;
        float attackRadius = 48.f;
        sf::Vector2f halfSize{ 50.f, 42.f }; // moitie de la taille (celle du png s'il y en a un)
    };
}
