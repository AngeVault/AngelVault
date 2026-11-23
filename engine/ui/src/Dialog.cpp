#include "../Include/components/Dialog.h"
#include "utils/camera_utils.h"
#include "scene/component_serializer.h"
#include "ecs/registry.h"
#include  <iostream>

#include "components/UIButton.h"
#include "components/UIText.h"
#include "components/collider2d.h"
#include "components/dont_destroy.h"
#include "components/geometry.h"
#include "components/map_transition.h"
#include "systems/camera_system.h"

inline DialogSystem::DialogSystem(sf::RenderTarget &renderTarget, Game &game) : m_game(game), m_rectShape(new sf::RectangleShape({100,50})), m_renderTarget(renderTarget), m_font(new sf::Font()), m_text(new sf::Text(*m_font))
{
    m_font->openFromFile("C:\\Windows\\Fonts\\arial.ttf");
    m_text->setFont(*m_font);
    m_text->setCharacterSize(50);
    m_text->setFillColor(sf::Color::White);
    m_text->setOutlineColor(sf::Color::Black);
    m_text->setOutlineThickness(10);
    priority = 14;
};

void DialogSystem::onUpdate(Registry &registry, float dt) {
    registry.view<Dialog, Transform2D>([&](Entity e, Dialog dialog, Transform2D transform) {
        const Entity entity = CameraSystem::activeCamera(registry);
        if (entity != NULL_ENTITY)
        {
            Camera2d& camera = registry.getComponent<Camera2d>(entity);
            Transform2D cameraPos = registry.getComponent<Transform2D>(e);

            m_text->setPosition(transform.getPosition() + cameraPos.getPosition());
            m_text->setString(dialog.dialog);
            m_renderTarget.draw(*m_text);
        }
        });
    }