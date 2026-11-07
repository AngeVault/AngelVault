//
// Created by p.pagliazzo on 02/11/2026.
//
#include "systems/camera_system.h"
#include "math/interpolation.h"
#include "ecs/registry.h"


sf::View CameraSystem::makeView(Entity e, Registry& registry, Camera2d& camera, sf::Vector2u windowSize) const
{
    sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
    sf::Vector2f half =
    {
        windowSize.x * 0.5f / camera.zoom,
        windowSize.y * 0.5f / camera.zoom
    };
    return sf::View(sf::FloatRect(
        { position.x - half.x, position.y - half.y },
        { half.x * 2.f, half.y * 2.f }
    ));
}

void CameraSystem::onUpdate(Registry& registry, float dt)
{
    registry.view<Camera2d>([&](Entity e, Camera2d& camera)
    {
        const sf::Vector2u viewportSize = m_window.getSize();

        if (camera.targetZoom != camera.zoom)
        {
            camera.zoom =  Math::moveTowards(camera.zoom, camera.targetZoom, camera.zoomLerpSpeed *dt) ;
        }



        m_window.setView(makeView(e,registry,camera,viewportSize));
    });

}

