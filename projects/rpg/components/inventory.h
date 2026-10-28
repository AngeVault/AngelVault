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

struct Inventory
{
    int size = 10;
    Inventory() =delete;
    Inventory(const Inventory&) =delete;
    Inventory(int _size) : size(_size) {};
    std::array<CaseInventory,size> inventory;
};


template<>
inline ReflectedComponent reflectComponent<Inventory>(Inventory& rb) {
    return { "Inventory", {
	            { "size",       PropertyInfo::Type::Int, &rb.size       }
        } };
}