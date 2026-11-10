//
// Created by p.pagliazzo on 10/11/2026.
//
#include "../systems/steeringSeek_system.h"
#include "../components/steering_component.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include <iostream>

void steeringSeekSystem::onStart(Registry& registry)
{
    ISystem::onStart(registry);
}
