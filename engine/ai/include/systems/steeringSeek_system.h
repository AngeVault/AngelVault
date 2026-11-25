//
// Created by p.pagliazzo on 10/11/2026.
//

#pragma once
#include "ecs/system.h"


class SteeringSeekSystem : public ISystem
{
public:
    SteeringSeekSystem(){priority = 4;};
    void onStart(Registry& registry) override;
    void onUpdate(Registry& registry, float dt) override;
private:
};
