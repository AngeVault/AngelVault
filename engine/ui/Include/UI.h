#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "../../core/include/components/transform2d.h"
#include <SFML/Graphics.hpp>
#include <cfloat>
#include <algorithm>
#include "components/UIButton.h"
#include "components/UIText.h"
#include "game_loop.h"




class SystemUI : public ISystem {
private:
    Game& m_game;
    sf::Font* m_font;
    sf::Text* m_text;
    sf::RenderTarget& m_renderTarget;
    sf::RenderWindow& m_window;
public:
    SystemUI(sf::RenderTarget& renderTarget, Game& game, sf::RenderWindow& window);
    void onUpdate(Registry& registry, float dt) override;
};


