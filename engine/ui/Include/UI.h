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




class SystemUI : public ISystem {
private:
    sf::Font m_font;
    sf::Text m_text;
    sf::RenderWindow& m_renderTarget;
public:
    SystemUI(sf::RenderWindow& renderTarget);
    void onUpdate(Registry& registry, float dt) override;
    void Render(Entity& entity, Registry& registry);
};


