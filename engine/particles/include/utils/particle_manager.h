#pragma once

#include <components/particle_emitter.h>
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <algorithm>
#include <cstdint>

/**
 * @brief Facade pratique pour manipuler les émetteurs de particules dans un ECS.
 *
 * Cette classe ne contient aucun état global. Elle agit comme une API de
 * commodité au-dessus du `Registry` et permet de créer, attacher, activer,
 * interrompre ou déclencher des effets de particules sans devoir gérer le
 * cycle ECS directement.
 *
 * @details
 * Les effets restent de simples composants et entités du Registry. Cela permet
 * de conserver les particules cohérentes avec le moteur de scène, de les mettre
 * à jour dans le système dédié, et de les supprimer proprement quand elles
 * sont terminées.
 */
class ParticleManager {
public:
    /**
     * @brief Attache un émetteur de particules à une entité existante.
     *
     * @param registry Référence du registre ECS dans lequel l'entité est stockée.
     * @param entity Entité cible à laquelle ajouter l'émetteur.
     * @param config Configuration de l'émetteur à appliquer.
     * @return `true` si l'attachement a réussi, `false` sinon.
     *
     * @details
     * La fonction échoue si :
     * - l'entité n'est pas valide ;
     * - l'entité a déjà un composant `ParticleEmitter` ou `ParticleEmitterRuntime` ;
     * - elle est déjà dans un état de destruction demandée.
     *
     * Si la cible n'a pas de `Transform2D`, il est ajouté automatiquement afin
     * de pouvoir positionner les particules émises.
     */
    static bool attachEmitter(Registry& registry, Entity entity, const ParticleEmitter& config) {
        if (!registry.isValid(entity) || registry.hasComponent<ParticleEmitter>(entity)
            || registry.hasComponent<ParticleEmitterRuntime>(entity))
            return false;
        if (!registry.hasComponent<Transform2D>(entity))
            registry.addComponent<Transform2D>(entity, Transform2D{});
        registry.addComponent<ParticleEmitter>(entity, config);
        ParticleEmitterRuntime runtime;
        runtime.pendingBurstCount = config.burstCount;
        runtime.randomState = config.randomSeed != 0 ? config.randomSeed
            : (entity.index * 747796405u + entity.generation * 2891336453u + 277803737u);
        if (runtime.randomState == 0) runtime.randomState = 1;
        registry.addComponent<ParticleEmitterRuntime>(entity, runtime);
        return true;
    }

    /**
     * @brief Crée une nouvelle entité autonome possédant un émetteur de particules.
     *
     * @param registry Référence du registre ECS.
     * @param position Position initiale de l'émetteur dans le monde.
     * @param config Configuration du nouvel émetteur.
     * @return Identifiant de l'entité créée.
     *
     * @details
     * L'entité créée contient automatiquement :
     * - un `Transform2D` pour la position et l'orientation ;
     * - un `ParticleEmitter` pour la configuration ;
     * - un `ParticleEmitterRuntime` pour l'état de simulation.
     */
    static Entity createEmitter(Registry& registry, sf::Vector2f position,
        const ParticleEmitter& config) {
        const Entity entity = registry.createEntity();
        Transform2D transform;
        transform.setPosition(position);
        registry.addComponent<Transform2D>(entity, transform);
        attachEmitter(registry, entity, config);
        return entity;
    }

    /**
     * @brief Crée un effet ponctuel unique qui se détruit automatiquement à la fin.
     *
     * @param registry Référence du registre ECS.
     * @param position Position de création de l'effet.
     * @param config Paramètres de l'émetteur.
     * @param count Nombre de particules à générer immédiatement.
     * @return Identifiant de l'entité créee pour ce burst.
     *
     * @details
     * Cette méthode transforme la configuration en un émetteur de type burst :
     * - `emissionRate = 0.f` pour empêcher l'émission continue ;
     * - `burstCount = count` pour générer le nombre demandé ;
     * - `emitting = false` pour ne pas faire tourner le système en continu ;
     * - `duration = 0.f` pour une durée de vie immédiate ;
     * - `destroyWhenFinished = true` pour supprimer l'émetteur quand toutes les
     *   particules sont mortes.
     */
    static Entity emitBurst(Registry& registry, sf::Vector2f position,
        ParticleEmitter config, std::uint32_t count) {
        config.emissionRate = 0.f;
        config.burstCount = count;
        config.emitting = false;
        config.duration = 0.f;
        config.destroyWhenFinished = true;
        return createEmitter(registry, position, config);
    }

    /**
     * @brief Ajoute une rafale supplémentaire à un émetteur déjà actif.
     *
     * @param registry Référence du registre.
     * @param emitter Entité émettrice concernée.
     * @param count Nombre de particules à ajouter à la file d'attente.
     * @return `true` si la demande a été acceptée, `false` sinon.
     *
     * @details
     * La fonction augmente `pendingBurstCount` sans dépasser la limite de
     * `UINT32_MAX`. Si l'émetteur est déjà marqué comme `destroyRequested`,
     * l'ajout est refusé pour éviter de créer des particules après la fin.
     */
    static bool queueBurst(Registry& registry, Entity emitter, std::uint32_t count) {
        if (!registry.isValid(emitter)
            || !registry.hasComponent<ParticleEmitterRuntime>(emitter))
            return false;
        auto& runtime = registry.getComponent<ParticleEmitterRuntime>(emitter);
        if (runtime.destroyRequested) return false;
        runtime.pendingBurstCount = count > UINT32_MAX - runtime.pendingBurstCount
            ? UINT32_MAX : runtime.pendingBurstCount + count;
        return true;
    }

    /**
     * @brief Active ou désactive l'émission d'un émetteur.
     *
     * @param registry Référence du registre.
     * @param emitter Entité à modifier.
     * @param enabled `true` pour activer l'émission, `false` pour la suspendre.
     * @return `true` si l'état a été modifié, `false` sinon.
     *
     * @details
     * Si l'émetteur est déjà marqué pour destruction (`destroyRequested`), il
     * ne peut plus être réactivé.
     */
    static bool setEmitting(Registry& registry, Entity emitter, bool enabled) {
        if (!registry.isValid(emitter) || !registry.hasComponent<ParticleEmitter>(emitter))
            return false;
        if (registry.hasComponent<ParticleEmitterRuntime>(emitter)
            && registry.getComponent<ParticleEmitterRuntime>(emitter).destroyRequested)
            return false;
        registry.getComponent<ParticleEmitter>(emitter).emitting = enabled;
        return true;
    }

    /**
     * @brief Demande l'arrêt de l'émetteur après la fin des particules actuelles.
     *
     * @param registry Référence du registre.
     * @param emitter Entité émettrice concernée.
     * @param destroyWhenFinished Si `true`, l'émetteur est détruit après sa fin.
     * @return `true` si la demande a été prise en compte, `false` sinon.
     *
     * @details
     * Cette fonction ne détruit pas immédiatement l'entité. Elle met simplement
     * `emitting` à `false` et active `destroyWhenFinished` pour que le système
     * de particules supprime proprement l'émetteur quand toutes ses particules
     * sont mortes.
     */
    static bool stop(Registry& registry, Entity emitter, bool destroyWhenFinished = true) {
        if (!registry.isValid(emitter) || !registry.hasComponent<ParticleEmitter>(emitter))
            return false;
        if (registry.hasComponent<ParticleEmitterRuntime>(emitter)
            && registry.getComponent<ParticleEmitterRuntime>(emitter).destroyRequested)
            return false;
        ParticleEmitter& config = registry.getComponent<ParticleEmitter>(emitter);
        config.emitting = false;
        config.destroyWhenFinished = destroyWhenFinished;
        return true;
    }
};