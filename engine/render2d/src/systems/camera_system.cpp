//
// Created by p.pagliazzo on 02/11/2026.
//
#include "systems/camera_system.h"

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
            const float worldWidth = camera.worldMax.x - camera.worldMin.x;
            const float worldHeight = camera.worldMax.y - camera.worldMin.y;
            sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();
            if (camera.targetLock == NULL_ENTITY)
            {


                position.x += (camera.panSpeedKeyboard * camera.zoom *dt) * Input::getAxis("Horizontal");
                position.y += (camera.panSpeedKeyboard * camera.zoom *dt) * Input::getAxis("Vertical");
                if (camera.zoomLocked == false)
                {
                    camera.targetZoom += (camera.zoomStep *dt) * Input::getAxis("ZoomWheel") ;
                }

                if (camera.targetZoom != camera.zoom)
                {
                    camera.zoom =  Math::moveTowards(camera.zoom, camera.targetZoom, camera.zoomLerpSpeed *dt) ;
                }

            }
            else
            {
                if (registry.isValid(camera.targetLock))
                {
                    float shiftAdaptatifX = camera.shiftSpeed;
                    float shiftAdaptatifY = camera.shiftSpeed;
                    sf::Vector2f positionCible = registry.getComponent<Transform2D>(camera.targetLock).getPosition();
                    auto& rb = registry.getComponent<RigidBody2D>(camera.targetLock);
                    if (rb.velocity.x > 0.f)
                    {
                        positionCible.x += camera.additionalAdvancement;
                    }
                    else if (rb.velocity.x < 0.f)
                    {
                        positionCible.x -= camera.additionalAdvancement;
                    }
                    shiftAdaptatifX = std::fabsf(positionCible.x - position.x)* camera.shiftSpeedMultiplicateur * camera.shiftSpeed;
                    if (rb.velocity.y > 0.f)
                    {
                        positionCible.y += camera.additionalAdvancement;
                    }
                    else if (rb.velocity.y < 0.f)
                    {
                        positionCible.y -= camera.additionalAdvancement;
                    }
                    shiftAdaptatifY = std::fabsf(positionCible.y - position.y)* camera.shiftSpeedMultiplicateur * camera.shiftSpeed;
                    if (shiftAdaptatifX < camera.shiftSpeed) shiftAdaptatifX = camera.shiftSpeed;
                    if (shiftAdaptatifY < camera.shiftSpeed) shiftAdaptatifY = camera.shiftSpeed;
                    //position = registry.getComponent<Transform2D>(camera.targetLock).getPosition();

                    position.x = Math::moveTowards(position.x, positionCible.x, shiftAdaptatifX*dt);
                    position.y = Math::moveTowards(position.y, positionCible.y, shiftAdaptatifY*dt);
                    if (camera.zoomLocked == false)
                    {
                        camera.targetZoom += (camera.zoomStep *dt) * Input::getAxis("ZoomWheel") ;
                    }
                    if (camera.targetZoom != camera.zoom)
                    {
                        camera.zoom =  Math::moveTowards(camera.zoom, camera.targetZoom, camera.zoomLerpSpeed*dt);
                    }
                }
                else
                {
                    camera.targetLock = NULL_ENTITY;
                }

            }

            if (position != registry.getComponent<Transform2D>(e).getPosition())
            {
                registry.getComponent<Transform2D>(e).setPosition(position);
            }
            if (viewportSize.x > 0 && viewportSize.y > 0 && worldWidth > 0.f && worldHeight > 0.f) {
                    // Empêche le zoom arrière d'élargir la vue au-delà de la carte.
                    const float fitZoom = std::max(
                        static_cast<float>(viewportSize.x) / worldWidth,
                        static_cast<float>(viewportSize.y) / worldHeight);
                    camera.minZoom = std::min(fitZoom, camera.maxZoom);
                    camera.zoom = std::max(camera.zoom, camera.minZoom);
                    camera.targetZoom = std::clamp(camera.targetZoom,
                    camera.minZoom, camera.maxZoom);
                }
                clampPosition(e,registry,camera,viewportSize);
                m_window.setView(makeView(e,registry,camera,viewportSize));
        });

    }

void CameraSystem::clampPosition(Entity e, Registry& registry, Camera2d& camera, sf::Vector2u windowSize)
{
    if (camera.worldMax.x <= camera.worldMin.x || camera.worldMax.y <= camera.worldMin.y) return;

    float halfW = (windowSize.x * 0.5f) /camera.zoom;
    float halfH = (windowSize.y * 0.5f) /camera.zoom;

    float minX = camera.worldMin.x + halfW;
    float maxX = camera.worldMax.x - halfW;
    float minY = camera.worldMin.y + halfH;
    float maxY = camera.worldMax.y - halfH;

    sf::Vector2f position = registry.getComponent<Transform2D>(e).getPosition();

    if (minX > maxX) position.x = (camera.worldMin.x + camera.worldMax.x) * 0.5f;
    else position.x = std::clamp(position.x, minX, maxX);
    if (minY > maxY) position.y = (camera.worldMin.y + camera.worldMax.y) * 0.5f;
    else position.y = std::clamp(position.y, minY, maxY);
    registry.getComponent<Transform2D>(e).setPosition(position);
}
