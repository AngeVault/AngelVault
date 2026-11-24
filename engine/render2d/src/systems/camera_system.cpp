#include "systems/camera_system.h"

#include "math/easing.h"
#include "math/interpolation.h"

#include <climits>
#include <cmath>

sf::Vector2u CameraSystem::screenSize(const sf::RenderTarget& target)
{
    const bool reference = s_referenceSize.x > 0 && s_referenceSize.y > 0;
    return reference ? s_referenceSize : target.getSize();
}

sf::Vector2f CameraSystem::visibleSize(float zoom, sf::Vector2u screen)
{
    const float z = zoom > 0.f ? zoom : 1.f;
    return { screen.x / z, screen.y / z };
}

Entity CameraSystem::activeCamera(Registry& registry)
{
    Entity best = NULL_ENTITY;
    int bestPriority = INT_MIN;
    registry.view<Camera2d, Transform2D>([&](Entity e, Camera2d& camera, Transform2D&)
        {
            if (camera.active && (best == NULL_ENTITY || camera.priority > bestPriority))
            {
                bestPriority = camera.priority;
                best = e;
            }
        });
    return best;
}

void CameraSystem::onUpdate(Registry& registry, float dt)
{
    const Entity e = activeCamera(registry);
    if (e != NULL_ENTITY)
    {
        Camera2d& camera = registry.getComponent<Camera2d>(e);
        const Pose target{
            registry.getComponent<Transform2D>(e).getPosition(),
            camera.zoom > 0.f ? camera.zoom : 1.f,
            camera.rotation };

        // Changement de caméra : fondu depuis la vue précédente
        if (!(e == m_current))
        {
            m_current = e;
            m_blendTime = m_hasPose ? camera.blendTime : 0.f;
            m_blendElapsed = 0.f;
            m_blendFrom = m_pose;
        }

        Pose pose = target;
        if (m_blendElapsed < m_blendTime)
        {
            m_blendElapsed += dt;
            const float t = Ease::smoothStep(m_blendElapsed / m_blendTime);
            pose.center = Math::lerp(m_blendFrom.center, target.center, t);
            pose.zoom = std::exp(Math::lerp(std::log(m_blendFrom.zoom), std::log(target.zoom), t));   // zoom régulier
            pose.rotation = Math::lerpAngle(m_blendFrom.rotation, target.rotation, t);
        }
        m_pose = pose;
        m_hasPose = true;

        // Effets de la frame (tremblement...) par-dessus
        const float zoom = pose.zoom * (camera.effectZoom > 0.f ? camera.effectZoom : 1.f);
        const sf::Vector2u screen = screenSize(m_target);
        sf::View view(pose.center + camera.effectOffset, visibleSize(zoom, screen));
        view.setRotation(sf::degrees(pose.rotation + camera.effectRotation));
        if (screen != m_target.getSize()) view.setViewport(letterbox(m_target.getSize(), screen));
        m_target.setView(view);
        s_view = view;
        s_targetSize = m_target.getSize();
    }
    else
    {
        m_current = NULL_ENTITY;
    }

    // Les effets sont recalculés à chaque frame
    registry.view<Camera2d>([](Entity, Camera2d& c)
        {
            c.effectOffset = { 0.f, 0.f };
            c.effectRotation = 0.f;
            c.effectZoom = 1.f;
        });
}

sf::Vector2f CameraSystem::screenToWorld(sf::Vector2i pixel)
{
    // comme sf::RenderTarget::mapPixelToCoords, avec la dernière vue
    const sf::FloatRect vp = s_view.getViewport();
    const sf::Vector2f size(s_targetSize);
    if (size.x <= 0.f || size.y <= 0.f) return sf::Vector2f(pixel);
    const sf::Vector2f origin{ vp.position.x * size.x, vp.position.y * size.y };
    const sf::Vector2f extent{ vp.size.x * size.x, vp.size.y * size.y };
    const sf::Vector2f ndc{
        -1.f + 2.f * (static_cast<float>(pixel.x) - origin.x) / extent.x,
         1.f - 2.f * (static_cast<float>(pixel.y) - origin.y) / extent.y };
    return s_view.getInverseTransform().transformPoint(ndc);
}

sf::Vector2i CameraSystem::worldToScreen(sf::Vector2f point)
{
    const sf::FloatRect vp = s_view.getViewport();
    const sf::Vector2f size(s_targetSize);
    const sf::Vector2f ndc = s_view.getTransform().transformPoint(point);
    return {
        static_cast<int>(std::lround((ndc.x + 1.f) / 2.f * vp.size.x * size.x + vp.position.x * size.x)),
        static_cast<int>(std::lround((-ndc.y + 1.f) / 2.f * vp.size.y * size.y + vp.position.y * size.y)) };
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
