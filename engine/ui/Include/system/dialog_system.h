#pragma once
#include "ecs/system.h"
#include "game_loop.h"
#include "../../../engine/gameplay/include/systems/interaction_system.h"

class DialogSystem : public ISystem
{
private:
    Game& m_game;
public:
    DialogSystem(Game& game, InteractionSystem& interaction);
    void use(Registry& registry, Entity entityA, Entity entityB);
    void onUpdate(Registry &registry, float dt) override;
};

