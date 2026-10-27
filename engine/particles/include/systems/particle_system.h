#pragma once

#include <ecs/registry.h>
#include <ecs/system.h>
#include <components/particle.h>
#include <components/particle_emitter.h>
#include <components/transform2d.h>
#include <SFML/System/Angle.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Système ECS responsable de la création, de la mise à jour et de la
 * destruction des particules.
 *
 * Ce système effectue plusieurs étapes à chaque frame :
 * 1. Analyse les émetteurs de particules et calcule le nombre de particules à
 *    créer.
 * 2. Crée les nouvelles entités particules sans invalider les vues ECS en cours
 *    d'itération.
 * 3. Met à jour la vie, la vitesse, la position et la rotation des particules
 *    existantes.
 * 4. Détecte les particules expirées et les émetteurs terminés pour les
 *    détruire proprement.
 *
 * @note Les nouvelles entités sont créées après l'itération des émetteurs afin
 * de ne pas invalider les vues du Registry qui contiennent Transform2D.
 */
class ParticleSystem : public ISystem {
public:
    /**
     * @brief Constructeur par défaut.
     *
     * @details Définit une priorité de mise à jour relativement élevée pour que
     * les effets visuels soient traités avant ou pendant les autres systèmes
     * dépendants de l'affichage.
     */
    ParticleSystem() { priority = 3; }

    /**
     * @brief Met à jour le système particules pour un pas de temps donné.
     *
     * @param registry Référence au registre ECS contenant les entités
     *                 `Transform2D`, `ParticleEmitter` et `Particle`.
     * @param dt Durée du pas de simulation, en secondes. Les valeurs négatives
     *           sont normalisées à zéro.
     *
     * @details
     * Le flux de mise à jour est le suivant :
     * - remise à zéro des lots d'émission;
     * - calcul des particules demandées pour chaque émetteur;
     * - création des particules en batch;
     * - mise à jour de l'âge et du mouvement des particules vivantes;
     * - destruction différée des particules expirées;
     * - recalcul des compteurs de particules vivantes;
     * - destruction des émetteurs ayant terminé leur cycle si nécessaire.
     */
    void onUpdate(Registry& registry, float dt) override {
        dt = std::max(0.f, dt);
        m_batches.clear();

        /**
         * @brief Itère sur tous les émetteurs de particules pour planifier leur
         * émission.
         *
         * On calcule :
         * - le temps écoulé de l'émetteur;
         * - la durée utile de cette frame;
         * - les particules dues à une rafale immédiate;
         * - les particules émises grâce au taux de génération continu.
         *
         * Les particules sont stockées dans `m_batches` pour être créées ensuite.
         */
        registry.view<Transform2D, ParticleEmitter, ParticleEmitterRuntime>(
            [&](Entity emitterEntity, Transform2D& transform, ParticleEmitter& emitter,
                ParticleEmitterRuntime& runtime) {
                const float previousElapsed = runtime.elapsed;
                runtime.elapsed += dt;

                const bool withinDuration = emitter.duration < 0.f
                    || previousElapsed < emitter.duration;
                const float emissionDt = emitter.duration < 0.f
                    ? dt : std::clamp(emitter.duration - previousElapsed, 0.f, dt);

                std::uint32_t requested = 0;
                const std::uint32_t perUpdateLimit = std::max(1u, emitter.maxParticlesPerUpdate);
                const std::uint32_t burst = std::min(runtime.pendingBurstCount, perUpdateLimit);
                runtime.pendingBurstCount -= burst;
                requested += burst;

                if (emitter.emitting && withinDuration && emitter.emissionRate > 0.f && emissionDt > 0.f) {
                    runtime.emissionAccumulator += static_cast<double>(emitter.emissionRate)
                        * static_cast<double>(emissionDt);
                    const double whole = std::floor(runtime.emissionAccumulator);
                    const std::uint32_t budget = perUpdateLimit - requested;
                    const auto available = static_cast<std::uint64_t>(std::min(
                        whole, static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
                    const std::uint32_t continuous = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(available, budget));
                    requested += continuous;
                    // Les pointes d'émission sont bornées : on ne crée pas une dette de particules
                    // qui provoquerait une longue série de frames coûteuses après un ralentissement.
                    runtime.emissionAccumulator -= whole;
                }

                if (requested > 0) {
                    m_batches.push_back({ emitterEntity, transform.getPosition(),
                        transform.getRotation().asRadians(), transform.depth, requested });
                }
            });

        /**
         * @brief Crée les particules planifiées dans les batches.
         *
         * La validation de l'émetteur garantit que l'entité est toujours valide
         * et possède bien les composants requis avant qu'une particule soit créée.
         */
        for (const SpawnBatch& batch : m_batches) {
            if (!registry.isValid(batch.emitter)
                || !registry.hasComponent<ParticleEmitter>(batch.emitter)
                || !registry.hasComponent<ParticleEmitterRuntime>(batch.emitter))
                continue;
            ParticleEmitter& emitter = registry.getComponent<ParticleEmitter>(batch.emitter);
            ParticleEmitterRuntime& runtime = registry.getComponent<ParticleEmitterRuntime>(batch.emitter);
            for (std::uint32_t i = 0; i < batch.count; ++i)
                spawnParticle(registry, batch, emitter, runtime);
        }

        m_expiredEntities.clear();

        /**
         * @brief Met à jour l'état physique et visuel des particules vivantes.
         *
         * Pour chaque particule :
         * - l'âge augmente;
         * - si la durée de vie est dépassée, la particule est marquée comme expirée;
         * - la vitesse est modifiée par l'accélération;
         * - la position est décalée selon la vitesse;
         * - la rotation est mise à jour selon la vitesse angulaire.
         */
        registry.view<Transform2D, Particle>([&](Entity entity, Transform2D& transform, Particle& particle) {
            if (particle.expired) return;
            particle.age += dt;
            if (particle.age >= particle.lifetime) {
                particle.expired = true;
                m_expiredEntities.push_back(entity);
                return;
            }

            particle.velocity += particle.acceleration * dt;
            transform.move(particle.velocity * dt);
            particle.rotation += particle.angularVelocity * dt;
            transform.setRotation(sf::degrees(particle.rotation));
        });

        // La destruction est différée par Registry jusqu'à flushPendingDestroys().
        for (Entity entity : m_expiredEntities)
            registry.destroyEntity(entity);

        // Recalcule le nombre vivant en un seul passage. Cela reste correct si le jeu
        // supprime un effet manuellement et évite de lier le compteur aux callbacks globaux.
        m_liveCounts.clear();
        registry.view<Particle>([&](Entity, Particle& particle) {
            if (particle.expired || particle.emitter.index == UINT32_MAX) return;
            const std::uint64_t key = (static_cast<std::uint64_t>(particle.emitter.generation) << 32)
                | particle.emitter.index;
            ++m_liveCounts[key];
        });

        m_finishedEmitters.clear();

        /**
         * @brief Détruit les émetteurs dont la fin de cycle est atteinte.
         *
         * Un émetteur est supprimé si :
         * - il est configuré pour se détruire à la fin ;
         * - il n'émet plus, ou sa durée d'émission est terminée ;
         * - aucune particule vivante n'est encore associée à l'émetteur ;
         * - aucune destruction n'a déjà été demandée.
         */
        registry.view<ParticleEmitter, ParticleEmitterRuntime>(
            [&](Entity entity, ParticleEmitter& emitter, ParticleEmitterRuntime& runtime) {
                const std::uint64_t key = (static_cast<std::uint64_t>(entity.generation) << 32)
                    | entity.index;
                const auto live = m_liveCounts.find(key);
                runtime.liveParticleCount = live == m_liveCounts.end() ? 0 : live->second;
                const bool durationEnded = emitter.duration >= 0.f && runtime.elapsed >= emitter.duration;
                if (emitter.destroyWhenFinished && (!emitter.emitting || durationEnded)
                    && runtime.pendingBurstCount == 0 && runtime.liveParticleCount == 0
                    && !runtime.destroyRequested) {
                    runtime.destroyRequested = true;
                    m_finishedEmitters.push_back(entity);
                }
            });
        for (Entity emitter : m_finishedEmitters)
            registry.destroyEntity(emitter);
    }

private:
    /**
     * @brief Représente un lot de particules à créer à partir d'un même émetteur.
     *
     * @details Chaque batch contient la position, l'orientation, la profondeur et
     * le nombre de particules à instancier.
     */
    struct SpawnBatch {
        Entity emitter;
        sf::Vector2f position;
        float rotationRadians;
        float depth;
        std::uint32_t count;
    };

    std::vector<SpawnBatch> m_batches;       ///< Lots de particules à créer durant cette frame
    std::vector<Entity> m_expiredEntities;    ///< Particules marquées comme expirées
    std::vector<Entity> m_finishedEmitters;   ///< Émetteurs terminés à détruire
    std::unordered_map<std::uint64_t, std::uint32_t> m_liveCounts; ///< Nombre vivant par émetteur

    /**
     * @brief Génère un nombre pseudo-aléatoire basé sur l'état interne de l'émetteur.
     *
     * @param runtime État courant du générateur aléatoire.
     * @return Valeur aléatoire non signée sur 32 bits.
     */
    static std::uint32_t nextRandom(ParticleEmitterRuntime& runtime) {
        std::uint32_t value = runtime.randomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        runtime.randomState = value == 0 ? 1u : value;
        return runtime.randomState;
    }

    /**
     * @brief Génère un flottant compris entre 0 et 1, de façon uniforme.
     *
     * @param runtime État du générateur.
     * @return Valeur aléatoire dans l'intervalle [0, 1).
     */
    static float randomUnit(ParticleEmitterRuntime& runtime) {
        return static_cast<float>(nextRandom(runtime) >> 8) / 16777215.f;
    }

    /**
     * @brief Génère un flottant aléatoire dans un intervalle donné.
     *
     * @param runtime État du générateur.
     * @param minimum Borne inférieure.
     * @param maximum Borne supérieure.
     * @return Valeur aléatoire dans [minimum, maximum].
     */
    static float randomRange(ParticleEmitterRuntime& runtime, float minimum, float maximum) {
        if (maximum < minimum) std::swap(minimum, maximum);
        return minimum + (maximum - minimum) * randomUnit(runtime);
    }

    /**
     * @brief Applique une rotation 2D à un vecteur.
     *
     * @param value Vecteur à transformer.
     * @param radians Angle de rotation en radians.
     * @return Vecteur après rotation.
     */
    static sf::Vector2f rotate(sf::Vector2f value, float radians) {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return { value.x * cosine - value.y * sine, value.x * sine + value.y * cosine };
    }

    /**
     * @brief Crée une particule unique dans le registre ECS.
     *
     * @param registry Référence du registre.
     * @param batch Lot d'émission source.
     * @param emitter Paramètres de l'émetteur.
     * @param runtime État d'exécution de l'émetteur.
     *
     * @details
     * La particule est générée à partir :
     * - d'une forme de spawn (rectangle ou cercle),
     * - d'une direction aléatoire dans l'angle configuré,
     * - d'une vitesse aléatoire,
     * - d'une durée de vie, d'une accélération et d'une vitesse angulaire variées.
     */
    static void spawnParticle(Registry& registry, const SpawnBatch& batch,
        const ParticleEmitter& emitter, ParticleEmitterRuntime& runtime) {
        const float pi = 3.14159265358979323846f;
        sf::Vector2f localOffset{};

        if (emitter.spawnShape == ParticleEmitter::SpawnShape::Rectangle) {
            localOffset = {
                randomRange(runtime, -std::abs(emitter.spawnArea.x) * 0.5f,
                    std::abs(emitter.spawnArea.x) * 0.5f),
                randomRange(runtime, -std::abs(emitter.spawnArea.y) * 0.5f,
                    std::abs(emitter.spawnArea.y) * 0.5f)
            };
        }
        else if (emitter.spawnShape == ParticleEmitter::SpawnShape::Circle) {
            const float angle = randomRange(runtime, 0.f, 2.f * pi);
            const float radius = std::sqrt(randomUnit(runtime)) * std::max(0.f, emitter.spawnRadius);
            localOffset = { std::cos(angle) * radius, std::sin(angle) * radius };
        }

        const sf::Vector2f origin = batch.position + rotate(localOffset, batch.rotationRadians);
        const float direction = randomRange(runtime, emitter.angleMinDegrees, emitter.angleMaxDegrees)
            * pi / 180.f + batch.rotationRadians;
        const float speed = randomRange(runtime, emitter.speedMin, emitter.speedMax);

        Particle particle;
        particle.emitter = batch.emitter;
        particle.velocity = { std::cos(direction) * speed, std::sin(direction) * speed };
        particle.acceleration = emitter.acceleration;
        particle.lifetime = std::max(0.001f,
            randomRange(runtime, emitter.lifetimeMin, emitter.lifetimeMax));
        particle.angularVelocity = randomRange(runtime,
            emitter.angularVelocityMin, emitter.angularVelocityMax);

        Entity entity = registry.createEntity();
        Transform2D transform;
        transform.setPosition(origin);
        transform.setRotation(sf::degrees(particle.rotation));
        transform.depth = batch.depth;
        registry.addComponent<Transform2D>(entity, transform);
        registry.addComponent<Particle>(entity, std::move(particle));
    }
};