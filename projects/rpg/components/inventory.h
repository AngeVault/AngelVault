//
// Created by p.pagliazzo on 28/10/2026.
//

#pragma once

#include <ecs/entity.h>
#include <array>
#include "ecs/reflect.h"


struct CaseInventory
{
    unsigned int quantity;
    Entity entity = NULL_ENTITY;
};

struct InventoryComponent
{
    int size = 10;
    InventoryComponent() =delete;
    InventoryComponent(const InventoryComponent&) =delete;
    InventoryComponent(int _size) : size(_size) {};
    std::array<CaseInventory,size> inventory;
};


template<>
inline ReflectedComponent reflectComponent<InventoryComponent>(InventoryComponent& rb) {
    return { "Inventory", {
	            { "size",       PropertyInfo::Type::Int, &rb.size       }
        } };
}