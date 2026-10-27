#pragma once

#include <SFML/System/Vector2.hpp>
#include <ecs/entity.h>

/**
 * @brief Représente une particule dans le système ECS de particules.
 *
 * Une particule est une entité classique du moteur : elle porte généralement
 * un état de transformation ainsi qu'un comportement de vie, de mouvement et
 * d'animation. Les textures associées sont partagées et conservées tant qu'une
 * particule les utilise.
 *
 * @note L'entité émettrice peut être utilisée pour relier une particule à sa
 * source de création (ex. système d'attaque, feu, explosion, etc.).
 */
struct Particle {
    /**
     * @brief Identifiant de l'entité émettrice de cette particule.
     *
     * @details Si la valeur vaut `NULL_ENTITY`, cela signifie que la particule
     * n'est rattachée à aucun émetteur connu.
     */
    Entity emitter = NULL_ENTITY;

    /**
     * @brief Vitesse linéaire actuelle de la particule.
     *
     * @details Représentée en pixels par seconde, selon les axes X et Y.
     * Elle est mise à jour à chaque frame par le système de simulation.
     */
    sf::Vector2f velocity{};

    /**
     * @brief Accélération appliquée à la particule.
     *
     * @details Représentée en pixels par seconde². Elle influe sur l'évolution
     * de la vitesse au fil du temps.
     */
    sf::Vector2f acceleration{};

    /**
     * @brief Âge actuel de la particule, en secondes.
     *
     * @details Cet âge augmente avec le temps jusqu'à atteindre `lifetime`.
     */
    float age = 0.f;

    /**
     * @brief Durée de vie totale de la particule, en secondes.
     *
     * @details Une fois `age >= lifetime`, la particule est généralement
     * marquée comme expirée et retirée du système.
     */
    float lifetime = 1.f;

    /**
     * @brief Rotation actuelle de la particule, en degrés.
     *
     * @details Cette valeur représente l'orientation visuelle de la particule
     * dans le monde 2D.
     */
    float rotation = 0.f; // degrés

    /**
     * @brief Vitesse de rotation angulaire de la particule.
     *
     * @details Représentée en degrés par seconde. Elle permet d'animer la
     * rotation au fil du temps.
     */
    float angularVelocity = 0.f; // degrés par seconde

    /**
     * @brief Indique si la particule a expiré.
     *
     * @details Lorsque cette valeur est `true`, le système de particules doit
     * généralement la supprimer ou la recycler pour de nouvelles instances.
     */
    bool expired = false;
};