#pragma once

#include "../components/buildingComp.h"
#include "../components/buildingVisual.h"
#include "building_texture.h"
#include "../components/geometry.h"
#include "../components/selectable.h"
#include "../components/unitComp.h"
#include "../components/unitVisuals.h"
#include <components/name.h>
#include <components/rigid_body2d.h>
#include <components/collider2d.h>
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <algorithm>
#include <optional>

namespace Rts {
    inline Entity createUnit(Registry& registry, ECS::Unit::Type type,
        ECS::Unit::Team team, sf::Vector2f position) {
        Entity entity = registry.createEntity();

        Transform2D transform;
        transform.setScale({ 0.5f, 0.5f });
        transform.setPosition(position);
        registry.addComponent<Transform2D>(entity, transform);
        registry.addComponent<Geometry>(entity, Geometry::SpriteData{ "player", sf::IntRect({ 0, 0 }, { 81, 70 }), unitTint(type, team) }, 1);
        registry.addComponent<NameComponent>(entity, "unit_" + std::to_string(entity.index));
        registry.addComponent<ECS::Unit>(entity, ECS::makeUnit(type, team));
        registry.addComponent<ECS::Selectable>(entity, ECS::Selectable{ team == ECS::Unit::Team::Player, false });

        RigidBody2D body;
        body.useGravity = false;
        body.fixedRotation = true;
        registry.addComponent<RigidBody2D>(entity, body);
        registry.addComponent<Collider2D>(entity, Collider2D::makeBox({ 16.f, 16.f }, true));
        return entity;
    }

    inline Entity createBuilding(Registry& registry, ECS::Building::Type type,
        sf::Vector2f position, bool completed = false) {
        Entity entity = registry.createEntity();

        Transform2D transform;
        transform.setPosition(position);
        registry.addComponent<Transform2D>(entity, transform);

        Geometry::ShapeStyle style;
        style.outlineColor = sf::Color(35, 30, 24);
        style.outlineThickness = 3.f;
        sf::Vector2f halfSize{ 50.f, 42.f };
        switch (type) {
        case ECS::Building::Type::Headquarters:
            halfSize = { 58.f, 48.f };
            style.fillColor = sf::Color(125, 85, 55);
            break;
        case ECS::Building::Type::Farm:
            halfSize = { 46.f, 34.f };
            style.fillColor = sf::Color(90, 155, 75);
            break;
        case ECS::Building::Type::Barracks:
            style.fillColor = sf::Color(145, 85, 70);
            break;
        case ECS::Building::Type::Tower:
            halfSize = { 30.f, 38.f };
            style.fillColor = sf::Color(128, 138, 160);
            break;
        }
        // un png dans assets/game/materials/buildings donne sa taille au batiment, sinon on garde le carre
        const std::optional<sf::Vector2i> textureSize = buildingTextureSize(type);
        ECS::BuildingVisual visual;
        if (textureSize) {
            halfSize = { textureSize->x * 0.5f, textureSize->y * 0.5f };
            visual.textureId = buildingSpriteId(type);
            visual.textureRect = sf::IntRect({ 0, 0 }, *textureSize);
            registry.addComponent<Geometry>(entity, Geometry::SpriteData{ visual.textureId, visual.textureRect, visual.tint }, 0);
        }
        else {
            registry.addComponent<Geometry>(entity, Geometry::BoxData{ halfSize, style }, 0);
        }
        registry.addComponent<NameComponent>(entity, "building_" + std::to_string(entity.index));
        ECS::Building building;
        building.type = type;
        building.constructionProgress = completed ? 1.f : 0.f;
        switch (type) {
        case ECS::Building::Type::Headquarters:
            building.pv = building.pvMax = 1800;
            building.attackRadius = 62.f;
            break;
        case ECS::Building::Type::Farm:
            building.pv = building.pvMax = 650;
            building.attackRadius = 48.f;
            break;
        case ECS::Building::Type::Barracks:
            building.pv = building.pvMax = 900;
            building.attackRadius = 52.f;
            break;
        case ECS::Building::Type::Tower:
            building.pv = building.pvMax = 600;
            building.attackRadius = 38.f;
            building.constructionDuration = 6.f;
            break;
        }
        building.halfSize = halfSize;
        if (textureSize) building.attackRadius = std::max(halfSize.x, halfSize.y) + 2.f;
        registry.addComponent<ECS::Building>(entity, building);
        registry.addComponent<ECS::BuildingVisual>(entity, visual);
        registry.addComponent<ECS::Selectable>(entity, ECS::Selectable{ true, false });
        registry.addComponent<Collider2D>(entity, Collider2D::makeBox(halfSize));
        return entity;
    }
}
