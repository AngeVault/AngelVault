#pragma once
#include <string>

struct CameraControl
{
    std::string kActionPanX = "CameraX";
    std::string kActionPanY = "CameraY";
    std::string kActionZoom = "CameraZoom";
    std::string kActionZoomWheel = "CameraZoomWheel";

    float zoom = 1.f;
    float minZoom = 0.25f;
    float maxZoom = 4.f;
};