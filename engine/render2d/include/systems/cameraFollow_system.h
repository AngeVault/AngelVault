//
// Created by pagli on 07/11/2026.
//

#pragma once
#include "ecs/system.h"


class CameraFollowSystem : public ISystem
{
public:
    CameraFollowSystem(){priority = -10;} ;
    void onUpdate(Registry& registry,float dt) override;;
};