#pragma once
// Outils d'interpolation (même esprit que Mathf / Vector2 d'Unity).
//
//     #include "math/interpolation.h"
//
//   Math::lerp(a, b, t)                    -> point à t (0..1) entre a et b
//   Math::moveTowards(pos, cible, vitesse * dt)  -> avance à VITESSE CONSTANTE, s'arrête pile sur la cible
//   Math::damp(pos, cible, 10.f, dt)       -> "lerp avec une vitesse" : rapide puis ralentit,
//                                              même résultat à 30 ou 144 FPS
//   Math::smoothDamp(pos, cible, vel, 0.3f, dt) -> ressort amorti (caméra qui suit en douceur)
//
// Tout marche avec float et sf::Vector2f. Fonctions sans état : l'état (vitesse de smoothDamp...)
// reste dans vos composants, comme le reste de l'ECS.

/**
 * @file interpolation.h
 * @brief Outils mathématiques pour interpoler, remapper et stabiliser les valeurs de mouvement.
 *
 * Ce module regroupe des fonctions analogues à celles de Unity/Mathf :
 * - interpolation linéaire et clamping
 * - déplacement vers une cible à vitesse constante
 * - déplacement avec amortissement dépendant du temps
 * - ressort amorti avec mémoire d'état
 * - gestion des angles et de la répétition de valeurs
 *
 * Les fonctions ne gardent pas d'état interne, sauf lorsque l'appelant décide
 * de conserver un vecteur de vitesse pour smoothDamp.
 */

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Math {

/**
 * @brief Constante PI.
 */
inline constexpr float kPi = 3.14159265358979f;

/**
 * @brief Coefficient de conversion degrés -> radians.
 */
inline constexpr float kDegToRad = kPi / 180.f;

/**
 * @brief Coefficient de conversion radians -> degrés.
 */
inline constexpr float kRadToDeg = 180.f / kPi;

/**
 * @brief Valeur utilisée pour éviter les divisions par zéro ou les erreurs numériques.
 */
inline constexpr float kEpsilon = 1e-6f;

// ---- Bases -------------------------------------------------------------------------------

/**
 * @brief Clamp une valeur dans [0, 1].
 *
 * @param t Valeur à normaliser.
 * @return `t` ramenée dans l'intervalle [0, 1].
 */
inline float clamp01(float t) { return std::clamp(t, 0.f, 1.f); }

/**
 * @brief Vérifie si deux flottants sont approximativement égaux.
 *
 * @param a Première valeur.
 * @param b Deuxième valeur.
 * @param epsilon Tolérance maximale.
 * @return true si la différence est inférieure ou égale à epsilon.
 */
inline bool approximately(float a, float b, float epsilon = 1e-5f) { return std::abs(a - b) <= epsilon; }

/**
 * @brief Calcule la longueur d'un vecteur 2D.
 *
 * @param v Vecteur.
 * @return Norme du vecteur.
 */
inline float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

/**
 * @brief Calcule la distance entre deux points 2D.
 *
 * @param a Point A.
 * @param b Point B.
 * @return Distance euclidienne entre A et B.
 */
inline float distance(sf::Vector2f a, sf::Vector2f b) { return length(b - a); }

/**
 * @brief Normalise un vecteur 2D.
 *
 * Si le vecteur est nul, retourne (0, 0).
 *
 * @param v Vecteur à normaliser.
 * @return Vecteur unitaire.
 */
inline sf::Vector2f normalize(sf::Vector2f v) {
    const float l = length(v);
    return l > kEpsilon ? v / l : sf::Vector2f{ 0.f, 0.f };
}

/**
 * @brief Limite la longueur d'un vecteur à une valeur maximale.
 *
 * @param v Vecteur source.
 * @param maxLength Longueur maximale autorisée.
 * @return Vecteur tronqué si nécessaire.
 */
inline sf::Vector2f clampLength(sf::Vector2f v, float maxLength) {
    const float l = length(v);
    return (l > maxLength && l > kEpsilon) ? v * (maxLength / l) : v;
}

// ---- Lerp ----------------------------------------------------------------------------------

/**
 * @brief Interpolation linéaire avec clamp de `t` dans [0, 1].
 *
 * @param a Valeur de départ.
 * @param b Valeur d'arrivée.
 * @param t Paramètre de progression, compris entre 0 et 1.
 * @return Valeur interpolée.
 */
inline float lerp(float a, float b, float t) { return a + (b - a) * clamp01(t); }
inline sf::Vector2f lerp(sf::Vector2f a, sf::Vector2f b, float t) { return a + (b - a) * clamp01(t); }

/**
 * @brief Interpolation linéaire non bornée.
 *
 * Permet d'obtenir des valeurs en dehors de [a, b] si `t` est hors de [0, 1].
 *
 * @param a Valeur de départ.
 * @param b Valeur d'arrivée.
 * @param t Paramètre de progression.
 * @return Valeur interpolée sans clamp.
 */
inline float lerpUnclamped(float a, float b, float t) { return a + (b - a) * t; }
inline sf::Vector2f lerpUnclamped(sf::Vector2f a, sf::Vector2f b, float t) { return a + (b - a) * t; }

/**
 * @brief Calcule la position relative de `value` entre `a` et `b`.
 *
 * Exemple : inverseLerp(10, 20, 15) = 0.5
 *
 * @param a Borne inférieure.
 * @param b Borne supérieure.
 * @param value Valeur dont on veut connaitre la position relative.
 * @return Valeur interpolante entre 0 et 1.
 */
inline float inverseLerp(float a, float b, float value) {
    return std::abs(b - a) > kEpsilon ? clamp01((value - a) / (b - a)) : 0.f;
}

/**
 * @brief Remappe une valeur d'une plage source vers une plage cible.
 *
 * @param value Valeur source.
 * @param inMin Borne minimum de la plage source.
 * @param inMax Borne maximum de la plage source.
 * @param outMin Borne minimum de la plage cible.
 * @param outMax Borne maximum de la plage cible.
 * @return Valeur remappée.
 */
inline float remap(float value, float inMin, float inMax, float outMin, float outMax) {
    return lerp(outMin, outMax, inverseLerp(inMin, inMax, value));
}

// ---- Vers une cible à vitesse constante ------------------------------------------------------

/**
 * @brief Avance progressivement vers une cible sans dépasser la distance autorisée.
 *
 * @param current Valeur actuelle.
 * @param target Valeur cible.
 * @param maxDelta Distance maximale de déplacement.
 * @return Nouvelle valeur.
 */
inline float moveTowards(float current, float target, float maxDelta) {
    if (std::abs(target - current) <= maxDelta) return target;
    return current + (target > current ? maxDelta : -maxDelta);
}

/**
 * @brief Déplace un vecteur 2D vers une cible à vitesse constante.
 *
 * @param current Position actuelle.
 * @param target Cible.
 * @param maxDistanceDelta Distance maximale de déplacement.
 * @return Nouvelle position.
 */
inline sf::Vector2f moveTowards(sf::Vector2f current, sf::Vector2f target, float maxDistanceDelta) {
    const sf::Vector2f delta = target - current;
    const float dist = length(delta);
    if (dist <= maxDistanceDelta || dist < kEpsilon) return target;
    return current + delta / dist * maxDistanceDelta;
}

// ---- Vers une cible en ralentissant (indépendant du framerate) ------------------------------

/**
 * @brief Amortissement de type "lerp avec vitesse", stable à n'importe quel FPS.
 *
 * La fonction parcourt une fraction du chemin restant à chaque frame.
 *
 * @param current Valeur actuelle.
 * @param target Valeur cible.
 * @param sharpness Force de l'amortissement ; plus elle est élevée, plus la convergence est rapide.
 * @param dt Temps écoulé depuis la dernière frame.
 * @return Nouvelle valeur.
 */
inline float damp(float current, float target, float sharpness, float dt) {
    return lerpUnclamped(current, target, 1.f - std::exp(-sharpness * dt));
}
inline sf::Vector2f damp(sf::Vector2f current, sf::Vector2f target, float sharpness, float dt) {
    return lerpUnclamped(current, target, 1.f - std::exp(-sharpness * dt));
}

/**
 * @brief Variante de damp avec une durée de demi-vie.
 *
 * Après `halfLife` secondes, il reste approximativement la moitié du chemin à parcourir.
 *
 * @param current Valeur actuelle.
 * @param target Valeur cible.
 * @param halfLife Durée de demi-vie.
 * @param dt Temps écoulé.
 * @return Nouvelle valeur.
 */
inline float dampHalfLife(float current, float target, float halfLife, float dt) {
    if (halfLife <= 0.f) return target;
    return lerpUnclamped(current, target, 1.f - std::exp2(-dt / halfLife));
}
inline sf::Vector2f dampHalfLife(sf::Vector2f current, sf::Vector2f target, float halfLife, float dt) {
    if (halfLife <= 0.f) return target;
    return lerpUnclamped(current, target, 1.f - std::exp2(-dt / halfLife));
}

// ---- Ressort amorti (SmoothDamp d'Unity) ---------------------------------------------------

/**
 * @brief Ressort amorti assimilable à SmoothDamp d'Unity.
 *
 * L'état `velocity` doit être conservé entre deux frames. Cela permet de suivre
 * une cible avec un mouvement doux sans rebond.
 *
 * @param current Valeur actuelle.
 * @param target Valeur cible.
 * @param velocity Vitesse interne du ressort.
 * @param smoothTime Temps de lissage.
 * @param dt Temps écoulé.
 * @param maxSpeed Vitesse maximale autorisée.
 * @return Nouvelle valeur.
 */
inline float smoothDamp(float current, float target, float& velocity, float smoothTime, float dt,
                        float maxSpeed = std::numeric_limits<float>::infinity()) {
    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.f / smoothTime;
    const float x = omega * dt;
    const float exp = 1.f / (1.f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float maxChange = maxSpeed * smoothTime;
    const float originalTarget = target;
    const float change = std::clamp(current - target, -maxChange, maxChange);
    target = current - change;
    const float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * exp;
    float result = target + (change + temp) * exp;
    // pas de dépassement de la cible
    if ((originalTarget - current > 0.f) == (result > originalTarget)) {
        result = originalTarget;
        velocity = dt > 0.f ? (result - originalTarget) / dt : 0.f;
    }
    return result;
}

/**
 * @brief Version Vector2 de smoothDamp.
 *
 * @param current Position actuelle.
 * @param target Position cible.
 * @param velocity Vitesse interne.
 * @param smoothTime Temps de lissage.
 * @param dt Temps écoulé.
 * @param maxSpeed Vitesse maximale autorisée.
 * @return Nouvelle position.
 */
inline sf::Vector2f smoothDamp(sf::Vector2f current, sf::Vector2f target, sf::Vector2f& velocity, float smoothTime,
                               float dt, float maxSpeed = std::numeric_limits<float>::infinity()) {
    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.f / smoothTime;
    const float x = omega * dt;
    const float exp = 1.f / (1.f + x + 0.48f * x * x + 0.235f * x * x * x);
    const sf::Vector2f originalTarget = target;
    const sf::Vector2f change = clampLength(current - target, maxSpeed * smoothTime);
    target = current - change;
    const sf::Vector2f temp = (velocity + change * omega) * dt;
    velocity = (velocity - temp * omega) * exp;
    sf::Vector2f result = target + (change + temp) * exp;
    // pas de dépassement de la cible
    const sf::Vector2f toTarget = originalTarget - current;
    const sf::Vector2f past = result - originalTarget;
    if (toTarget.x * past.x + toTarget.y * past.y > 0.f) {
        result = originalTarget;
        velocity = { 0.f, 0.f };
    }
    return result;
}

// ---- Angles (en degrés) ----------------------------------------------------------------------

/**
 * @brief Calcule la différence angulaire la plus courte entre deux angles.
 *
 * La valeur est ramenée dans l'intervalle [-180, 180].
 *
 * @param current Angle actuel.
 * @param target Angle cible.
 * @return Différence la plus courte.
 */
inline float deltaAngle(float current, float target) {
    float d = std::fmod(target - current, 360.f);
    if (d > 180.f) d -= 360.f;
    else if (d < -180.f) d += 360.f;
    return d;
}

/**
 * @brief Interpolation d'angle en prenant le chemin le plus court.
 *
 * @param a Angle de départ.
 * @param b Angle d'arrivée.
 * @param t Paramètre de progression.
 * @return Angle interpolé.
 */
inline float lerpAngle(float a, float b, float t) { return a + deltaAngle(a, b) * clamp01(t); }

/**
 * @brief Déplace un angle vers une cible en limitant le delta maximal.
 *
 * @param current Angle actuel.
 * @param target Angle cible.
 * @param maxDelta Variation maximale autorisée.
 * @return Nouveau angle.
 */
inline float moveTowardsAngle(float current, float target, float maxDelta) {
    const float d = deltaAngle(current, target);
    if (std::abs(d) <= maxDelta) return target;
    return current + (d > 0.f ? maxDelta : -maxDelta);
}

/**
 * @brief Amortissement d'angle compatible avec le framerate.
 *
 * @param current Angle actuel.
 * @param target Angle cible.
 * @param sharpness Puissance de l'amortissement.
 * @param dt Temps écoulé.
 * @return Nouveau angle.
 */
inline float dampAngle(float current, float target, float sharpness, float dt) {
    return current + deltaAngle(current, target) * (1.f - std::exp(-sharpness * dt));
}

/**
 * @brief Calcule l'angle en degrés d'un vecteur 2D.
 *
 * En SFML, l'axe Y descend, donc (0, 1) correspond à 90°.
 *
 * @param v Vecteur source.
 * @return Angle en degrés.
 */
inline float angleOf(sf::Vector2f v) { return std::atan2(v.y, v.x) * kRadToDeg; }

/**
 * @brief Crée un vecteur 2D depuis un angle en degrés.
 *
 * @param degrees Angle en degrés.
 * @param len Longueur du vecteur.
 * @return Vecteur directionnel.
 */
inline sf::Vector2f fromAngle(float degrees, float len = 1.f) {
    return { std::cos(degrees * kDegToRad) * len, std::sin(degrees * kDegToRad) * len };
}

// ---- Divers ------------------------------------------------------------------------------------

/**
 * @brief Fait osciller une valeur entre 0 et `length`.
 *
 * Exemple : pingPong(1.5f, 1.f) retourne 0.5.
 *
 * @param t Valeur source.
 * @param len Amplitude de l'oscillation.
 * @return Valeur oscillante.
 */
inline float pingPong(float t, float len) {
    if (len <= 0.f) return 0.f;
    const float m = std::fmod(std::abs(t), len * 2.f);
    return len - std::abs(m - len);
}

/**
 * @brief Repète une valeur dans [0, length[.
 *
 * Exemple : repeat(370, 360) = 10.
 *
 * @param t Valeur source.
 * @param len Longueur de la plage.
 * @return Valeur répétée dans le domaine.
 */
inline float repeat(float t, float len) {
    if (len <= 0.f) return 0.f;
    return std::clamp(t - std::floor(t / len) * len, 0.f, len);
}

} // namespace Math