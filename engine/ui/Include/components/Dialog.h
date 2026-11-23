#pragma once
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include "game_loop.h"
#include "systems/collision_system.h"
#include "components/geometry.h"
#include "components/UIText.h"

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

struct Dialog
{
    std::string dialog;
};

template<>
inline ReflectedComponent reflectComponent<Dialog>(Dialog& d) {
    return { "Dialog", {
            {"Text", PropertyInfo::Type::String, &d.dialog }
    }
    };
}