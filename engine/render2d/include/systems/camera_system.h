#pragma once
// Affiche la caméra active (Camera2d) dans l'image du jeu.
//
// Ordre des systèmes caméra (voir registerCameraSystems, systems/camera_systems.h) :
//   -1 CameraControlSystem  pan / zoom (entrées)
//    4 CameraFollowSystem   suit sa cible (après physique et scripts)
//    5 CameraBoundsSystem   reste dans sa zone
//    6 CameraShakeSystem    tremblement (effets de la frame)
//    7 CameraSystem         vue finale : caméra active + effets + fondu entre caméras
//    8+ rendu (tilemap, entités)
#include "ecs/system.h"
#include "ecs/entity.h"
#include "ecs/registry.h"
#include "components/camera2d.h"
#include "components/transform2d.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/View.hpp>

class CameraSystem : public ISystem
{
public:
    explicit CameraSystem(sf::RenderTarget& target) : m_target(target) { priority = 7; }

    // Écran de référence (0 x 0 = la taille de la fenêtre, le cas de rpg.exe).
    // L'éditeur met la taille du projet : en jeu, la caméra montre la même zone que son cadre dans la vue scène,
    // quelle que soit la taille du panneau (bandes sur les côtés si le format n'est pas le même).
    static void setReferenceSize(sf::Vector2u size) { s_referenceSize = size; }
    static sf::Vector2u referenceSize() { return s_referenceSize; }
    // Taille d'écran vue par les caméras : la référence, sinon la cible
    static sf::Vector2u screenSize(const sf::RenderTarget& target);
    // Zone de la fenêtre (en 0..1) qui garde le format de l'écran de référence, centrée
    static sf::FloatRect letterbox(sf::Vector2u window, sf::Vector2u reference);

    // Caméra affichée : active, avec Transform2D, la plus grande priorité (NULL_ENTITY si aucune)
    static Entity activeCamera(Registry& registry);
    // Taille du monde visible avec ce zoom
    static sf::Vector2f visibleSize(float zoom, sf::Vector2u screen);

    // Dernière vue appliquée : pixels de l'image du jeu <-> monde (clic souris...)
    static const sf::View& currentView() { return s_view; }
    static sf::Vector2f screenToWorld(sf::Vector2i pixel);
    static sf::Vector2i worldToScreen(sf::Vector2f point);

    void onUpdate(Registry& registry, float dt) override;

private:
    struct Pose { sf::Vector2f center; float zoom = 1.f; float rotation = 0.f; };

    sf::RenderTarget& m_target;   // image du jeu (RenderTexture de Game)
    Entity m_current = NULL_ENTITY;
    Pose m_pose;                  // dernière vue appliquée
    Pose m_blendFrom;
    float m_blendTime = 0.f, m_blendElapsed = 0.f;
    bool m_hasPose = false;

    inline static sf::Vector2u s_referenceSize{ 0u, 0u };
    inline static sf::View s_view;
    inline static sf::Vector2u s_targetSize{ 0u, 0u };
};
