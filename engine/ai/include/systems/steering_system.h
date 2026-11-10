//
// Created by p.pagliazzo on 09/11/2026.
//

#pragma once
#include "ecs/system.h"

class SteeringSystem : public ISystem
{
public:
    void onStart(Registry& registry) override;
    SteeringSystem() { priority = 3; }
    void onUpdate(Registry& registry, float dt) override;
};