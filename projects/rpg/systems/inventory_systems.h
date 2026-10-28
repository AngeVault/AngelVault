//
// Created by p.pagliazzo on 28/10/2026.
//

#pragma once

#include "ecs/system.h"
#include "../components/inventory.h"
#include "ecs/entity.h"

class InventorySystem : public ISystem
{
public:
    InventorySystem() {priority = -10;} ;
private:

};

