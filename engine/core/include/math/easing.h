#pragma once
// Courbes d'easing : déforment un t (0..1) pour animer autrement qu'en ligne droite.
// À combiner avec un lerp :
//
//     #include "math/easing.h"
//     float t = timer / duree;                                   // 0 -> 1
//     pos = Math::lerp(depart, arrivee, Ease::outBack(t));        // dépasse un peu puis revient
//
//     // ou par nom (choix dans un PROPERTY, un fichier...) :
//     pos = Math::lerp(depart, arrivee, Ease::apply(Ease::Type::OutBounce, t));
//
// in = démarre doucement, out = finit doucement, inOut = les deux. Toutes : f(0) = 0, f(1) = 1.

/**
 * @file easing.h
 * @brief Fournit des fonctions d'easing pour modifier la courbe d'interpolation d'une animation.
 *
 * Les courbes d'easing sont utilisées pour rendre une interpolation plus naturelle :
 * - "in" : la progression commence lentement
 * - "out" : la progression finit lentement
 * - "inOut" : le mouvement est lent au début et à la fin
 *
 * Elles s'emploient typiquement avec Math::lerp ou Math::lerpUnclamped :
 * une valeur `t` de 0 à 1 est transformée en une valeur de progression modifiée,
 * puis interpolée entre deux points.
 */

#include "math/interpolation.h"

#include <cmath>

namespace Ease {

/**
 * @brief Courbe linéaire : progression identique tout au long du temps.
 *
 * @param t Temps normalisé entre 0 et 1.
 * @return Valeur transformée ; retourne `t`.
 */
inline float linear(float t) { return t; }

/**
 * @brief Courbe SmoothStep.
 *
 * Très utilisée en animation : lente au départ, rapide au milieu, lente à la fin.
 * Equivalent à Mathf.SmoothStep dans d'autres moteurs.
 *
 * @param t Temps normalisé entre 0 et 1.
 * @return Valeur ajustée.
 */
inline float smoothStep(float t) { t = Math::clamp01(t); return t * t * (3.f - 2.f * t); }

/**
 * @brief Courbe SmootherStep, plus douce que SmoothStep.
 *
 * @param t Temps normalisé entre 0 et 1.
 * @return Valeur ajustée.
 */
inline float smootherStep(float t) { t = Math::clamp01(t); return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

// Puissance 2, 3...

/**
 * @brief Easing quadratique, rapide au début.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inQuad(float t)    { return t * t; }

/**
 * @brief Easing quadratique, rapide à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outQuad(float t)   { return 1.f - (1.f - t) * (1.f - t); }

/**
 * @brief Easing quadratique, doux au début et à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inOutQuad(float t) { return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2.f) / 2.f; }

/**
 * @brief Easing cubique, qui accélère plus fortement.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inCubic(float t)    { return t * t * t; }

/**
 * @brief Easing cubique, plus doux à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outCubic(float t)   { return 1.f - std::pow(1.f - t, 3.f); }

/**
 * @brief Easing cubique doux au début et à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inOutCubic(float t) { return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f; }

/**
 * @brief Easing sinusoidal, lent au début.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inSine(float t)    { return 1.f - std::cos(t * Math::kPi / 2.f); }

/**
 * @brief Easing sinusoidal, lent à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outSine(float t)   { return std::sin(t * Math::kPi / 2.f); }

/**
 * @brief Easing sinusoidal doux en début et fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inOutSine(float t) { return -(std::cos(Math::kPi * t) - 1.f) / 2.f; }

/**
 * @brief Easing exponentiel, très lent au départ puis très rapide.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inExpo(float t)  { return t <= 0.f ? 0.f : std::pow(2.f, 10.f * t - 10.f); }

/**
 * @brief Easing exponentiel, rapide puis lent à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outExpo(float t) { return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t); }

/**
 * @brief Easing "Back" : recule légèrement avant de partir.
 *
 * Ce type d'easing simule un mouvement qui dépasse un peu puis revient.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inBack(float t)  { const float c1 = 1.70158f, c3 = c1 + 1.f; return c3 * t * t * t - c1 * t * t; }

/**
 * @brief Easing "Back" sortant : dépasse un peu avant l'arrivée.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outBack(float t) { const float c1 = 1.70158f, c3 = c1 + 1.f; return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f); }

/**
 * @brief Easing "Back" combiné, en douceur au début et à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inOutBack(float t) {
    const float c2 = 1.70158f * 1.525f;
    return t < 0.5f ? (std::pow(2.f * t, 2.f) * ((c2 + 1.f) * 2.f * t - c2)) / 2.f
                    : (std::pow(2.f * t - 2.f, 2.f) * ((c2 + 1.f) * (t * 2.f - 2.f) + c2) + 2.f) / 2.f;
}

/**
 * @brief Easing élastique, avec oscillation.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outElastic(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    const float c4 = (2.f * Math::kPi) / 3.f;
    return std::pow(2.f, -10.f * t) * std::sin((t * 10.f - 0.75f) * c4) + 1.f;
}

/**
 * @brief Easing élastique en entrée, complémentaire de outElastic.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inElastic(float t) { return 1.f - outElastic(1.f - t); }

// Bounce : rebondit à l'arrivée (une balle qui tombe)

/**
 * @brief Easing "Bounce" sortant : rebond à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float outBounce(float t) {
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.f / d1) return n1 * t * t;
    if (t < 2.f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

/**
 * @brief Easing "Bounce" entrant.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inBounce(float t) { return 1.f - outBounce(1.f - t); }

/**
 * @brief Easing "Bounce" entrant/sortant, rebond au milieu et à la fin.
 *
 * @param t Temps normalisé.
 * @return Valeur transformée.
 */
inline float inOutBounce(float t) {
    return t < 0.5f ? (1.f - outBounce(1.f - 2.f * t)) / 2.f : (1.f + outBounce(2.f * t - 1.f)) / 2.f;
}

// ---- Choix par valeur (pour un champ réglable) ------------------------------------------------

/**
 * @brief Types d'easing disponibles.
 *
 * Chaque valeur représente une courbe standard utilisée pour donner une sensation
 * particulière à une interpolation.
 */
enum class Type {
    Linear, SmoothStep, SmootherStep,
    InQuad, OutQuad, InOutQuad,
    InCubic, OutCubic, InOutCubic,
    InSine, OutSine, InOutSine,
    InExpo, OutExpo,
    InBack, OutBack, InOutBack,
    InElastic, OutElastic,
    InBounce, OutBounce, InOutBounce,
};

/**
 * @brief Applique une courbe d'easing à une valeur `t` entre 0 et 1.
 *
 * @param type Courbe à appliquer.
 * @param t Valeur temporelle ou de progression, normalement comprise entre 0 et 1.
 * @return Valeur transformée.
 */
inline float apply(Type type, float t) {
    t = Math::clamp01(t);
    switch (type) {
    case Type::Linear:       return linear(t);
    case Type::SmoothStep:   return smoothStep(t);
    case Type::SmootherStep: return smootherStep(t);
    case Type::InQuad:       return inQuad(t);
    case Type::OutQuad:      return outQuad(t);
    case Type::InOutQuad:    return inOutQuad(t);
    case Type::InCubic:      return inCubic(t);
    case Type::OutCubic:     return outCubic(t);
    case Type::InOutCubic:   return inOutCubic(t);
    case Type::InSine:       return inSine(t);
    case Type::OutSine:      return outSine(t);
    case Type::InOutSine:    return inOutSine(t);
    case Type::InExpo:       return inExpo(t);
    case Type::OutExpo:      return outExpo(t);
    case Type::InBack:       return inBack(t);
    case Type::OutBack:      return outBack(t);
    case Type::InOutBack:    return inOutBack(t);
    case Type::InElastic:    return inElastic(t);
    case Type::OutElastic:   return outElastic(t);
    case Type::InBounce:     return inBounce(t);
    case Type::OutBounce:    return outBounce(t);
    case Type::InOutBounce:  return inOutBounce(t);
    }
    return t;
}

/**
 * @brief Interpole entre deux valeurs en appliquant une courbe d'easing.
 *
 * @param a Valeur de départ.
 * @param b Valeur d'arrivée.
 * @param t Temps normalisé entre 0 et 1.
 * @param type Courbe d'easing à appliquer.
 * @return Valeur interpolée.
 */
inline float lerp(float a, float b, float t, Type type) { return Math::lerpUnclamped(a, b, apply(type, t)); }

/**
 * @brief Interpole entre deux vecteurs 2D en appliquant une courbe d'easing.
 *
 * @param a Point de départ.
 * @param b Point d'arrivée.
 * @param t Temps normalisé entre 0 et 1.
 * @param type Courbe d'easing à appliquer.
 * @return Vecteur interpolé.
 */
inline sf::Vector2f lerp(sf::Vector2f a, sf::Vector2f b, float t, Type type) {
    return Math::lerpUnclamped(a, b, apply(type, t));
}

} // namespace Ease