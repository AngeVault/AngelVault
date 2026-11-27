//
// Created by p.pagliazzo on 23/11/2026.
//

#pragma once
#include "ecs/system.h"


class SteeringFleeSystem : public ISystem
{
public:
    SteeringFleeSystem(){priority = 5;};
    void onStart(Registry& registry) override;
    void onUpdate(Registry& registry, float dt) override;
private:
};
