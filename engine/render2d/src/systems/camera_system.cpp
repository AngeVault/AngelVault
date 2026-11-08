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
            const bool reference = s_referenceSize.x > 0 && s_referenceSize.y > 0;
            const sf::Vector2u viewportSize = reference ? s_referenceSize : m_target.getSize();
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
                    sf::Vector2f velocity = {};

                    if (registry.hasComponent<RigidBody2D>(camera.targetLock))
                    {
                        velocity = registry.getComponent<RigidBody2D>(camera.targetLock).velocity;
                    }
                    if (velocity.x > 0.f)
                    {
                        positionCible.x += camera.additionalAdvancement;
                    }
                    else if (velocity.x < 0.f)
                    {
                        positionCible.x -= camera.additionalAdvancement;
                    }
                    shiftAdaptatifX = std::fabs(positionCible.x - position.x)* camera.shiftSpeedMultiplicateur * camera.shiftSpeed;
                    if (velocity.y > 0.f)
                    {
                        positionCible.y += camera.additionalAdvancement;
                    }
                    else if (velocity.y < 0.f)
                    {
                        positionCible.y -= camera.additionalAdvancement;
                    }
                    shiftAdaptatifY = std::fabs(positionCible.y - position.y)* camera.shiftSpeedMultiplicateur * camera.shiftSpeed;

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
                sf::View view = makeView(e,registry,camera,viewportSize);
                if (reference) view.setViewport(letterbox(m_target.getSize(), s_referenceSize));
                m_target.setView(view);
        });

    }

sf::FloatRect CameraSystem::letterbox(sf::Vector2u window, sf::Vector2u reference)
{
    if (window.x == 0 || window.y == 0 || reference.x == 0 || reference.y == 0) return { { 0.f, 0.f }, { 1.f, 1.f } };
    const float windowRatio = static_cast<float>(window.x) / static_cast<float>(window.y);
    const float referenceRatio = static_cast<float>(reference.x) / static_cast<float>(reference.y);
    if (windowRatio > referenceRatio) {   // fenêtre plus large : bandes à gauche et à droite
        const float w = referenceRatio / windowRatio;
        return { { (1.f - w) * 0.5f, 0.f }, { w, 1.f } };
    }
    const float h = windowRatio / referenceRatio;   // plus haute : bandes en haut et en bas
    return { { 0.f, (1.f - h) * 0.5f }, { 1.f, h } };
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
