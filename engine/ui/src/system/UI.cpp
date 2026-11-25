#include "system/UI.h"

#include "utils/camera_utils.h"
#include "scene/component_serializer.h"
#include  <iostream>
#include <numbers>

#include "components/UIButton.h"
#include "components/UIText.h"
#include "components/collider2d.h"
#include "components/Dialog.h"
#include "components/dont_destroy.h"
#include "components/geometry.h"
#include "components/map_transition.h"
#include "scene/scene_transition.h"



SystemUI::SystemUI(sf::RenderTarget& renderTarget, Game& game, sf::RenderWindow& window) : m_game(game), m_renderTarget(renderTarget), m_window(window), m_text(new sf::Text(game.font()))
{
    m_text->setCharacterSize(50);
    priority = 13;
    // collision.onCollision = [this](const CollisionInfo& info) { onTrigger(m_game.registry(), info); };
}


void SystemUI::onUpdate(Registry &registry, float dt) {
    sf::View worldView = m_renderTarget.getDefaultView();
    registry.view<UIButton, Transform2D>([&](Entity e, UIButton& ui, Transform2D transform) {
            if (!registry.hasComponent<Geometry>(e)) return;
                auto& geo = registry.getComponent<Geometry>(e);
            sf::Vector2f mousePos = Camera::screenToWorld(sf::Mouse::getPosition(m_window));
            sf::Vector2f buttonPos = transform.getPosition();
            if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && (ui.isTrigered && mousePos.x < buttonPos.x + ui.with && mousePos.y < buttonPos.y + ui.height && mousePos.x+ui.with > buttonPos.x && mousePos.y+ui.height > buttonPos.y)) {
                ui.triggerEvent = true;
            }
            registry.view<MapTransition, UIButton>([&](Entity buttonEntity, MapTransition transition, UIButton& ui) {
                if (!ui.triggerEvent) return;
                    changeScene(registry, m_game.assetDir(), transition.scene, transition.spawn);
                    ui.triggerEvent = false;
            });
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && (ui.isTrigered || mousePos.x < buttonPos.x + ui.with && mousePos.y < buttonPos.y + ui.height && mousePos.x+ui.with > buttonPos.x && mousePos.y+ui.height > buttonPos.y)) {
               ui.isTrigered = true;
               if (auto data = std::get_if<Geometry::BoxData>(&geo.data)) {
                   data->style.fillColor.a = 200;
                   ui.with = data->halfSize.x;
                   ui.height = data->halfSize.y;
               }
               else if (auto data = std::get_if<Geometry::CircleData>(&geo.data)) {
                   data->style.fillColor.a = 200;
               }
               else if (auto data = std::get_if<Geometry::PolygonData>(&geo.data)) {
                   data->style.fillColor.a = 200;
               }
           }
           else {
               if (auto data = std::get_if<Geometry::BoxData>(&geo.data)) {
                   data->style.fillColor.a = 255;
               }
               else if (auto data = std::get_if<Geometry::CircleData>(&geo.data)) {
                   data->style.fillColor.a = 255;
               }
               else if (auto data = std::get_if<Geometry::PolygonData>(&geo.data)) {
                   data->style.fillColor.a = 255;
               }
               ui.isTrigered = false;
           }
       });
    registry.view<UIText, Transform2D>([&](Entity e, UIText& ui, Transform2D& transform) {

        if (ui.isHUD)m_renderTarget.setView(worldView);
        m_text->setString(ui.text);
        transform.setPosition({transform.getPosition().x+ui.offset.x-(m_text->getGlobalBounds().size.x/2*!ui.isHUD), transform.getPosition().y+ui.offset.y});
        m_renderTarget.draw(*m_text, transform.getLocalMatrix());
        transform.setPosition({transform.getPosition().x-ui.offset.x+(m_text->getGlobalBounds().size.x/2*!ui.isHUD), transform.getPosition().y-ui.offset.y});
    });
}

