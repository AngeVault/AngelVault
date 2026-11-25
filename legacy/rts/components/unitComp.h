#pragma once

#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstdint>
#include <ecs/entity.h>
#include "ecs/reflect.h"

namespace ECS {

    struct Unit {
        enum class Type { Soldier, Melee, Archer, Builder, Healer };
        enum class Team { Player, Enemy };

        Type  type = Type::Soldier;
        Team  team = Team::Player;

        int   pv = 100;
        int   pvMax = 100;
        int   dmg = 10;
        float speed = 200.f;

        float attackRange = 40.f;
        float attackCooldown = 1.f;
        float attackTimer = 0.f;
        float targetSearchTimer = 0.f;
        Entity target = NULL_ENTITY;
        Entity constructionTarget = NULL_ENTITY;
        bool chargeOrder = false;

        bool hasDestination = false;
        sf::Vector2f destination{ 0.f, 0.f };
        uint32_t movementGroup = 0;

        bool canAttack = true;
        bool unpaid = false;   // pas payee, degats reduits
        bool visibleToPlayer = false;

        float healRange = 150.f;
        int   healAmount = 15;
        float healCooldown = 1.f;
        float healTimer = 0.f;
    };

    // salaire par paie selon le type d'unite
    inline int unitWage(Unit::Type type) {
        switch (type) {
        case Unit::Type::Archer:  return 3;
        case Unit::Type::Melee:   return 3;
        case Unit::Type::Healer:  return 2;
        case Unit::Type::Builder: return 1;
        case Unit::Type::Soldier:
        default:                  return 2;
        }
    }

    // degats reels, divises par 2 si pas payee
    inline int effectiveDamage(const Unit& u) {
        return u.unpaid ? std::max(1, u.dmg / 2) : u.dmg;
    }

    inline Unit makeUnit(Unit::Type type, Unit::Team team) {
        Unit u;
        u.type = type;
        u.team = team;
        switch (type) {
        case Unit::Type::Melee:
            u.pv = 180; u.pvMax = 180; u.dmg = 22; u.speed = 80.f;
            u.attackRange = 44.f; u.attackCooldown = 1.1f;
            break;
        case Unit::Type::Archer:
            u.pv = 70;  u.pvMax = 70;  u.dmg = 16; u.speed = 100.f;
            u.attackRange = 220.f; u.attackCooldown = 1.4f;
            break;
        case Unit::Type::Healer:
            u.pv = 110; u.pvMax = 110; u.dmg = 0; u.speed = 120.f;
            u.attackRange = 0.f; u.attackCooldown = 1.f;
            u.canAttack = false;
            u.healRange = 150.f; u.healAmount = 15; u.healCooldown = 1.f;
            break;
        case Unit::Type::Builder:
            u.pv = 80;  u.pvMax = 80;  u.dmg = 5;  u.speed = 110.f;
            u.attackRange = 24.f; u.attackCooldown = 1.f;
            break;
        case Unit::Type::Soldier:
        default:
            u.pv = 100; u.pvMax = 100; u.dmg = 12; u.speed = 100.f;
            u.attackRange = 40.f; u.attackCooldown = 0.8f;
            break;
        }
        return u;
    }

}
