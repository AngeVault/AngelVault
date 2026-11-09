#pragma once
#include "ecs/system.h"

class CameraControlSystem : public ISystem {
    CameraControlSystem() { priority = -11; } // avant le rendu

    void onUpdate(Registry& registry, float dt) override {
        registry.view<CameraControl, Camera2d>([&](Entity e, CameraControl& control, Camera2d& camera) {
            //TODO: gérer le zoom avec la molette de la souris (CameraControl::kActionZoomWheel)
        });
    }
    

};
