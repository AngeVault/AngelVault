#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "../../core/include/components/transform2d.h"
#include <SFML/Graphics.hpp>
#include <cfloat>
#include <algorithm>

struct UIText {
    std::string m_text;
    static UIText create(Registry& registry);
    void Render(Registry& registry, sf::RenderWindow& window);
};

struct UIRect {
    sf::IntRect m_rect;
    static UIRect create(Registry& registry);
    void Render(Registry& registry, sf::RenderWindow& window);
};


class SystemUI : public ISystem {
private:
    sf::RectangleShape m_spriteRect;
    sf::Font m_font;
    sf::Text m_text;
    std::vector<UIText> m_texts;
    std::vector<UIRect> m_rects;
public:
    SystemUI(Registry& registry);
    Entity addUIText(Registry& registry);
    Entity addUIRect(Registry& registry);
    void onUpdate(Registry& registry, float dt) override;
    void Render(Registry& registry, sf::RenderWindow& window);
};


