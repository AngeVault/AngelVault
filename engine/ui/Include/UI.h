#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "../../core/include/components/transform2d.h"
#include <SFML/Graphics.hpp>
#include <cfloat>
#include <algorithm>
#include "components/UIRect.h"
#include "components/UIText.h"




class SystemUI : public ISystem {
private:
    sf::RectangleShape m_spriteRect;
    sf::Font m_font;
    sf::Text m_text;
    sf::RenderTarget& m_renderTarget;
    std::vector<UIText> m_texts;
    std::vector<UIRect> m_rects;
public:
    SystemUI(sf::RenderTarget& renderTarget);
    Entity addUIText(Registry& registry);
    Entity addUIRect(Registry& registry);
    void onUpdate(Registry& registry, float dt) override;
    void Render(Registry& registry);
};


