#include "Ui.h"

#include "components/name.h"
#include "utils/camera_utils.h"
#include "scene/component_serializer.h"
#include  <iostream>

#include "components/dont_destroy.h"
#include "components/geometry.h"
#include "components/map_transition.h"
#include "scene/scene_transition.h"


SystemUI::SystemUI(sf::RenderWindow& renderTarget, Game& game) : m_game(game), m_renderTarget(renderTarget), m_font(new sf::Font()), m_text(new sf::Text(*m_font))
{
    m_font->openFromFile("C:/Users/n.escande/repo_CLion/AngelVault/out/build/x64-release/_deps/harfbuzz-src/util/gpu/default-font.ttf");
    m_text->setFont(*m_font);
    m_text->setCharacterSize(50);
    priority = 20;
}

void SystemUI::onUpdate(Registry &registry, float dt) {
    registry.view<UIButton, Transform2D>([&](Entity e, UIButton& ui, Transform2D transform) {
            if (!registry.hasComponent<Geometry>(e)) return;
                auto& geo = registry.getComponent<Geometry>(e);
            sf::Vector2f mousePos = Camera::screenToWorld(sf::Mouse::getPosition(m_renderTarget));
            sf::Vector2f buttonPos = transform.getPosition();
            if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && (ui.isTrigered && mousePos.x < buttonPos.x + ui.with && mousePos.y < buttonPos.y + ui.height && mousePos.x+ui.with > buttonPos.x && mousePos.y+ui.height > buttonPos.y)) {
                ui.triggerEvent = true;
            }
            registry.view<MapTransition, UIButton>([&](Entity buttonEntity, MapTransition transition, UIButton& ui) {
                if (!ui.triggerEvent) return;
                registry.view<DontDestroy>([&](Entity playerEntity, DontDestroy player) {
                    if (player.key != "Player") return ;
                    changeScene(registry, m_game.assetDir(), transition.scene, transition.spawn);
                    ui.triggerEvent = false;
                });
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
    registry.view<UIText, Transform2D>([&](Entity e, UIText& ui, Transform2D transform) {
        m_text->setString(ui.text);
        m_text->setPosition(transform.getPosition());
        m_renderTarget.draw(*m_text);
    });
}

