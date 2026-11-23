#pragma once
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include "game_loop.h"
#include "systems/collision_system.h"




class SystemUI : public ISystem {
private:
    Game& m_game;
    sf::Font* m_font;
    sf::Text* m_text;
    sf::RenderTarget& m_renderTarget;
    sf::RenderWindow& m_window;
public:
    SystemUI(sf::RenderTarget& renderTarget, Game& game, sf::RenderWindow& window);
    void onTrigger(Registry& registry, CollisionInfo info);
    void onUpdate(Registry& registry, float dt) override;
};


