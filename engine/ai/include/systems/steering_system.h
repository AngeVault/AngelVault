//
// Created by p.pagliazzo on 09/11/2026.
//

#pragma once
#include "ecs/system.h"


class steeringSystem : public ISystem
{
public:
    steeringSystem(){priority = -10;};
    void onStart(Registry& registry) override;
private:
};
