//
// Created by pagli on 07/11/2026.
//
#pragma once
#include "ecs/system.h"


class CameraControlSystem : public ISystem
{
public:
    static constexpr const char* kActionPanX = "CameraX";
    static constexpr const char* kActionPanY = "CameraY";
    static constexpr const char* kActionZoom = "Zoom";
    static constexpr const char* kActionZoomWheel = "ZoomWheel";

    CameraControlSystem(){priority = -10;} ;
    void onUpdate(Registry& registry,float dt) override;;
};