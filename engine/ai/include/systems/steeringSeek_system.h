//
// Created by p.pagliazzo on 10/11/2026.
//

#pragma once
#include "ecs/system.h"


class steeringSeekSystem : public ISystem
{
public:
    steeringSeekSystem(){priority = -10;};
    void onStart(Registry& registry) override;
private:
};
