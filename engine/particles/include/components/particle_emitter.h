#pragma once

#include <SFML/System/Vector2.hpp>
#include <cstdint>

/**
 * @brief Configuration d'un émetteur de particules.
 *
 * Cette structure décrit la manière dont l'émetteur produit des particules.
 * La logique d'exécution de l'émission est séparée dans ParticleEmitterRuntime
 * afin de garder la configuration pure et le state ECS distinct.
 */
struct ParticleEmitter {
    /**
     * @brief Forme utilisée pour générer la position d'apparition des particules.
     */
    enum class SpawnShape { Point, Rectangle, Circle };

    /**
     * @brief Nombre de particules émises par seconde.
     *
     * Une valeur à 0 désactive l'émission continue.
     */
    float emissionRate = 0.f;

    /**
     * @brief Nombre de particules à générer immédiatement au démarrage.
     *
     * Cette rafale est ajoutée à la première mise à jour ou au moment
     * de l'activation de l'émetteur.
     */
    std::uint32_t burstCount = 0;

    /**
     * @brief Durée totale de vie de l'émetteur en secondes.
     *
     * Une valeur strictement négative signifie un émetteur sans fin.
     */
    float duration = -1.f;

    /**
     * @brief Indique si l'émetteur est actuellement actif.
     *
     * Lorsqu'il vaut false, aucune particule n'est créée tant que l'état
     * n'est pas réactivé.
     */
    bool emitting = true;

    /**
     * @brief Demande la destruction de l'entité contenant l'émetteur
     * une fois l'émission terminée.
     */
    bool destroyWhenFinished = false;

    /**
     * @brief Nombre maximal de particules générées lors d'une seule mise à jour.
     *
     * Cela sert à limiter les coûts de calcul et de mémoire sur des
     * émissions très intenses.
     */
    std::uint32_t maxParticlesPerUpdate = 256;

    /**
     * @brief Forme de spawn utilisée pour déterminer la position initiale.
     */
    SpawnShape spawnShape = SpawnShape::Point;

    /**
     * @brief Taille du rectangle de spawn si spawnShape == Rectangle.
     *
     * Les composantes x et y correspondent respectivement à la largeur
     * et à la hauteur du rectangle.
     */
    sf::Vector2f spawnArea{};

    /**
     * @brief Rayon du cercle de spawn si spawnShape == Circle.
     */
    float spawnRadius = 0.f;

    /**
     * @brief Angle minimal de direction en degrés.
     *
     * Utilisé pour initialiser la vitesse de départ des particules.
     */
    float angleMinDegrees = 0.f;

    /**
     * @brief Angle maximal de direction en degrés.
     *
     * Les angles sont généralement interprétés dans l'intervalle
     * [angleMinDegrees, angleMaxDegrees].
     */
    float angleMaxDegrees = 360.f;

    /**
     * @brief Vitesse minimale initiale des particules.
     */
    float speedMin = 0.f;

    /**
     * @brief Vitesse maximale initiale des particules.
     */
    float speedMax = 0.f;

    /**
     * @brief Accélération appliquée à chaque particule au fil du temps.
     *
     * Ce vecteur est généralement ajouté à la vélocité de la particule
     * pendant ses mises à jour.
     */
    sf::Vector2f acceleration{};

    /**
     * @brief Durée de vie minimale d'une particule en secondes.
     */
    float lifetimeMin = 0.5f;

    /**
     * @brief Durée de vie maximale d'une particule en secondes.
     */
    float lifetimeMax = 1.5f;

    /**
     * @brief Vitesse angulaire minimale initiale.
     */
    float angularVelocityMin = 0.f;

    /**
     * @brief Vitesse angulaire maximale initiale.
     */
    float angularVelocityMax = 0.f;

    /**
     * @brief Graine aléatoire de l'émetteur.
     *
     * Une valeur à 0 indique que la graine doit être déterministe
     * et basée sur l'entité ou sur l'instance ECS associée.
     */
    std::uint32_t randomSeed = 0;
};

/**
 * @brief État d'exécution d'un émetteur de particules.
 *
 * Ce runtime sépare les données mutables de la configuration statique.
 * Cela permet de garder la structure de données ECS plus claire et
 * évite de corrompre les paramètres de configuration pendant le jeu.
 */
struct ParticleEmitterRuntime {
    /**
     * @brief Accumulateur utilisé pour gérer le taux d'émission.
     *
     * Il est incrémenté par le delta time et permet de produire des particules
     * selon un taux en particules par seconde.
     */
    double emissionAccumulator = 0.0;

    /**
     * @brief Nombre de particules restantes à émettre dans une rafale.
     *
     * Cette valeur est utilisée pour différer la création de particules
     * jusqu'à ce que la rafale ait été complètement drainée.
     */
    std::uint32_t pendingBurstCount = 0;

    /**
     * @brief Nombre actuel de particules vivantes émises par cet émetteur.
     *
     * Utile pour savoir quand un émetteur est saturé ou pour limiter
     * les créations si la limite est atteinte.
     */
    std::uint32_t liveParticleCount = 0;

    /**
     * @brief État interne du générateur pseudo-aléatoire.
     *
     * Permet d'obtenir des comportements déterministes à partir d'une graine
     * ou d'un état reproductible.
     */
    std::uint32_t randomState = 1;

    /**
     * @brief Temps écoulé depuis le démarrage de l'émetteur.
     */
    float elapsed = 0.f;

    /**
     * @brief Indique qu'une destruction a été demandée pour l'émetteur.
     *
     * Cela permet au système ECS de supprimer proprement l'entité ou
     * la composante quand l'émission est terminée.
     */
    bool destroyRequested = false;
};