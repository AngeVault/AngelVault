//
// Created by pagli on 07/11/2026.
//

#pragma once

#include "ecs/reflect.h"

struct CameraShake
{

};
template<>
inline ReflectedComponent reflectComponent<CameraShake>(CameraShake& rb) {
    return { "CameraShake", {

            } };
}
