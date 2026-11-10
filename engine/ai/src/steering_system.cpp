//
// Created by p.pagliazzo on 09/11/2026.
//

#include "systems/steering_system.h"
#include "components/steering_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"

void SteeringSystem::onUpdate(Registry& registry, float dt)
{
    registry.view<SteeringComponent, Transform2D>(
        [dt](Entity, SteeringComponent& steering, Transform2D& transform) {
            transform.move(steering.direction * steering.rapidite * dt);
        });
}
