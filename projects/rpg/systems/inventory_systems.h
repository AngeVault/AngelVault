//
// Created by p.pagliazzo on 28/10/2026.
//

#pragma once

#include "ecs/system.h"
#include "../components/inventory.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/name.h"
#include "../components/item.h"
#include <stdexcept>

class InventorySystem : public ISystem
{
public:
    InventorySystem() {priority = -10;} ;
    void onUpdate(Registry& registry,float dt) override
    {
        registry.view<InventoryComponent>([&](Entity e, InventoryComponent& inventoryComponent)
        {

        });
    }
    bool add (Entity e, InventoryComponent& inventoryComponent, Registry& registry)
    {
        if (registry.hasComponent<ItemComponent>(e))
        {
            int i = 0;
            while (inventoryComponent.size > i)
            {
                if (inventoryComponent.inventory[i].entity != NULL_ENTITY)
                {
                    NameComponent& itemName = registry.getComponent<NameComponent>(inventoryComponent.inventory[i].entity);
                    NameComponent& addaItemName = registry.getComponent<NameComponent>(e);
                    if (itemName.name == addaItemName.name)
                    {
                        ItemComponent& item = registry.getComponent<ItemComponent>(e);
                        if (item.stackMax > inventoryComponent.inventory[i].quantity)
                        {
                            inventoryComponent.inventory[i].quantity++;
                            registry.destroyEntity(e);
                            return true;
                        }
                    }
                }
                i++;
            }
            while (inventoryComponent.size > i)
            {
                if (inventoryComponent.inventory[i].entity == NULL_ENTITY)
                {
                    inventoryComponent.inventory[i].entity = e;
                    inventoryComponent.inventory[i].quantity++;
                    return true;
                }
                i++;
            }
            return false;
        }
        else
        {
            throw std::invalid_argument("Only Item in Inventory");
        }
    }
    Entity& substract (int index, InventoryComponent& inventoryComponent, Registry& registry)
    {
        if (inventoryComponent.inventory[index].quantity > 0)
        {
            Entity e = inventoryComponent.inventory[index].entity;
            inventoryComponent.inventory[index].quantity--;
            if (inventoryComponent.inventory[index].quantity == 0)
            {
                inventoryComponent.inventory[index].entity = NULL_ENTITY;
                return e;
            }
            else
            {
                e = registry.createEntity();
                //int b = registry.
            }

        }
    }
private:

};

