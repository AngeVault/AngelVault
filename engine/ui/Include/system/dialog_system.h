#pragma once
#include "ecs/system.h"
#include "game_loop.h"

class DialogSystem : public ISystem
{
private:
    Game& m_game;
    sf::RectangleShape* m_rectShape;
    sf::Font* m_font;
    sf::Text* m_text;
    sf::RenderTarget& m_renderTarget;
public:
    DialogSystem(sf::RenderTarget& renderTarget, Game& game);
    void onUpdate(Registry &registry, float dt) override;
};

