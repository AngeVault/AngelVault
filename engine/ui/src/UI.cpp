#include "Ui.h"

#include "components/name.h"
#include "scene/component_serializer.h"
#include  <iostream>


SystemUI::SystemUI(sf::RenderWindow& renderTarget) : m_renderTarget(renderTarget),m_spriteRect(sf::RectangleShape()), m_font(sf::Font()), m_text(sf::Text(m_font))
{
    m_font.openFromFile("C:/Users/Gamer/repos_Clion/AngelVault/out/build/x64-debug/_deps/harfbuzz-src/util/gpu/default-font.ttf");
    m_text.setFont(m_font);
    priority = 13;
}

void SystemUI::onUpdate(Registry &registry, float dt) {
  Render(registry);
}

void SystemUI::Render(Registry& registry) {
    registry.view<UIButton, Transform2D>([&](Entity e, UIButton& ui, Transform2D transform) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(m_renderTarget);
            sf::Vector2i buttonPos = static_cast<sf::Vector2i>(transform.getPosition());
                std::cout << buttonPos.x << ", " << buttonPos.y << std::endl;
                if (mousePos.x < buttonPos.x + ui.with && mousePos.y < buttonPos.y + ui.height && mousePos.x > buttonPos.x && mousePos.y > buttonPos.y) {
                ui.isTrigered = true;
                m_spriteRect.setFillColor(sf::Color(ui.r,ui.g,ui.b,100));
            }
            else {
                ui.isTrigered = false;
                m_spriteRect.setFillColor(sf::Color(ui.r,ui.g,ui.b,255));
            }
            m_spriteRect.setSize(sf::Vector2f(ui.with, ui.height));
            m_renderTarget.draw(m_spriteRect, transform.getLocalMatrix());
        });
    registry.view<UIText, Transform2D>([&](Entity e, UIText& ui, Transform2D transform) {
        m_text.setString(ui.text);
        m_renderTarget.draw(m_text, transform.getLocalMatrix());
    });
}

