#include "Ui.h"

#include "components/name.h"
#include "scene/component_serializer.h"


SystemUI::SystemUI(sf::RenderTarget& renderTarget) : m_renderTarget(renderTarget),m_spriteRect(sf::RectangleShape()), m_font(sf::Font()), m_text(sf::Text(m_font))
{
    priority = 13;
}

void SystemUI::onUpdate(Registry &registry, float dt) {
    Render(registry);
}

void SystemUI::Render(Registry& registry) {
    registry.view<UIRect, Transform2D>([&](Entity e, UIRect& ui, Transform2D transform) {
            m_renderTarget.draw(m_spriteRect, transform.getLocalMatrix());
        });
    registry.view<UIRect, Transform2D>([&](Entity e, UIRect& ui, Transform2D transform) {
        m_renderTarget.draw(m_text, transform.getLocalMatrix());
    });
}


UIRect UIRect::create(Registry &registry) {
    UIRect ui;
    return ui;
}
