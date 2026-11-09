#include "../../engine/ui/Include/UI.h"

#include <qimageiohandler.h>

#include "components/name.h"
#include "scene/component_serializer.h"


SystemUI::SystemUI(Registry &registry)
    :m_spriteRect(sf::RectangleShape()), m_font(sf::Font()), m_text(sf::Text(m_font))
{
    priority = 1;
}

Entity SystemUI::addUIText(Registry &registry) {
    Entity entity = registry.createEntity();
    Transform2D transform;
    registry.addComponent<NameComponent>(entity, "Text" + std::to_string(entity.index));
    registry.addComponent<Transform2D>(entity, transform);
    registry.addComponent<UIText>(entity, UIText::create(registry));
    return entity;
}

Entity SystemUI::addUIRect(Registry &registry) {
    Entity entity = registry.createEntity();
    Transform2D transform;
    registry.addComponent<NameComponent>(entity, "Text" + std::to_string(entity.index));
    registry.addComponent<Transform2D>(entity, transform);
    registry.addComponent<UIRect>(entity, UIRect::create(registry));
    return entity;
}

void SystemUI::onUpdate(Registry &registry, float dt) {

}

void SystemUI::Render(Registry &registry, sf::RenderWindow &window) {
    registry.view<UIRect>([&](Entity e, Transform2D transform, UIRect& ui) {
        m_spriteRect.setPosition(transform.getPosition());
            window.draw(m_spriteRect);
        });
        registry.view<UIRect>([&](Entity e, Transform2D transform, UIRect& ui) {
            m_text.setPosition(transform.getPosition());
            window.draw(m_text);
        });
}

UIText UIText::create(Registry& registry) {
    UIText ui;
    ui.m_text = "Lorem ipsum";
    return ui;
}




UIRect UIRect::create(Registry &registry) {
    UIRect ui;
    ui.m_rect = sf::IntRect({{0,0},{0,0}});
    return ui;
}
