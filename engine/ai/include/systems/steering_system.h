//
// Created by p.pagliazzo on 09/11/2026.
//

#pragma once
#include "ecs/system.h"


class CameraSystem : public ISystem
{
public:
    CameraSystem(){priority = -10;};
    void onUpdate(Registry& registry,float dt) override;
private:
};
