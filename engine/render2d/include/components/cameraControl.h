//
// Created by pagli on 07/11/2026.
//

#pragma once

#include "ecs/reflect.h"

struct CameraControl
{
    // state
    float minZoom = 0.25f;
    float maxZoom = 4.f;
    bool zoomLocked = false;

    // speed
    float panSpeedKeyboard = 1000.f;
    float panSpeedGamepad = 100.f;



    void setZoomLoked(bool locked)
    {
        zoomLocked = locked;
    };
};
template<>
inline ReflectedComponent reflectComponent<CameraControl>(CameraControl& rb) {
    return { "CameraControl", {
	            { "minZoom",       PropertyInfo::Type::Float, &rb.minZoom       },
                { "maxZoom",    PropertyInfo::Type::Float, &rb.maxZoom    },
                { "zoomLocked",    PropertyInfo::Type::Bool, &rb.zoomLocked    },
                { "panSpeedKeyboard",       PropertyInfo::Type::Float, &rb.panSpeedKeyboard       },
                { "panSpeedGamepad",    PropertyInfo::Type::Float, &rb.panSpeedGamepad    },
            } };
}