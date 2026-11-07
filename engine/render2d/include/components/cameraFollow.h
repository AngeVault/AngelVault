//
// Created by pagli on 07/11/2026.
//
#pragma once

#include "ecs/entity.h"
#include "ecs/reflect.h"

struct CameraFollow
{
    // state
    Entity targetLock = NULL_ENTITY;

     // speed
    float shiftSpeed = 40.f;
    float shiftSpeedMultiplicateur = 0.1f;
    float additionalAdvancement = 200.f;

    void setTarget(Entity e)
    {
        targetLock = e;
    };

};
template<>
inline ReflectedComponent reflectComponent<CameraFollow>(CameraFollow& rb) {
    return { "CameraFollow", {
	            { "shiftSpeed",       PropertyInfo::Type::Float, &rb.shiftSpeed       },
                { "shiftSpeedMultiplicateur", PropertyInfo::Type::Float, &rb.shiftSpeedMultiplicateur },
                { "additionalAdvancement",   PropertyInfo::Type::Float, &rb.additionalAdvancement   },
        } };
}