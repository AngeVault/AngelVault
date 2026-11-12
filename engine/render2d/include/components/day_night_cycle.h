#pragma once
// Cycle jour / nuit : à poser sur UNE entité de la scène (la première trouvée est utilisée).
// DayNightSystem fait avancer l'heure et teinte l'écran avec la couleur du moment.
//
// Couleurs : chaque couleur tient de son heure jusqu'à l'heure suivante, puis passe à la suivante
// en `transitionHours`. Ex. (1 h) : 4 h-5 h nuit -> aube, 7 h-8 h aube -> jour, 17 h-18 h jour -> soir...
//
// Depuis le code du jeu :  registry.view<DayNightCycle>([](Entity, DayNightCycle& c) { if (c.isNight()) ... });
#include "ecs/reflect.h"

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct DayNightCycle {
    float hour = 8.f;                  // heure courante, 0 .. 24 (dans la scène : l'heure de départ)
    float minutesPerGameHour = 10.f;   // 1 h en jeu = 10 min réelles
    bool paused = false;

    float dawnHour = 5.f;
    float dayHour = 8.f;
    float duskHour = 18.f;
    float nightHour = 21.f;
    float transitionHours = 1.f;

    sf::Color dawnColor{ 255, 190, 150 };
    sf::Color dayColor{ 255, 255, 255 };
    sf::Color duskColor{ 255, 150, 110 };
    sf::Color nightColor{ 70, 80, 140 };

    float vignette = 0.5f;             // bords assombris la nuit (0 = aucun)

    void advance(float dt) {
        if (paused || minutesPerGameHour <= 0.f) return;
        hour = std::fmod(hour + dt / (minutesPerGameHour * 60.f), 24.f);
        if (hour < 0.f) hour += 24.f;
    }

    sf::Color ambient() const {
        struct Key { float hour; sf::Color color; };
        std::array<Key, 4> keys{ { { dawnHour, dawnColor }, { dayHour, dayColor },
                                   { duskHour, duskColor }, { nightHour, nightColor } } };
        std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.hour < b.hour; });
        // Dernière clé passée (avant la première de la journée : celle de la veille)
        std::size_t i = keys.size() - 1;
        for (std::size_t k = 0; k < keys.size(); ++k)
            if (hour >= keys[k].hour) i = k;
        const Key& from = keys[i];
        const Key& to = keys[(i + 1) % keys.size()];
        const float span = std::fmod(to.hour - from.hour + 24.f, 24.f);
        const float elapsed = std::fmod(hour - from.hour + 24.f, 24.f);
        const float blend = std::clamp(transitionHours, 0.001f, std::max(span, 0.001f));
        const float t = std::clamp((elapsed - (span - blend)) / blend, 0.f, 1.f);
        auto mix = [t](std::uint8_t a, std::uint8_t b) { return std::uint8_t(std::lround(a + (b - a) * t)); };
        return { mix(from.color.r, to.color.r), mix(from.color.g, to.color.g), mix(from.color.b, to.color.b) };
    }

    // 0 = plein jour, 1 = couleur de nuit
    float darkness() const {
        auto luma = [](sf::Color c) { return (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.f; };
        const float day = luma(dayColor), night = luma(nightColor);
        if (day - night < 0.001f) return 0.f;
        return std::clamp((day - luma(ambient())) / (day - night), 0.f, 1.f);
    }

    bool isNight() const { return hour >= nightHour || hour < dawnHour; }
};

// Champs simples (les couleurs sont sérialisées à part : PropertyInfo n'a pas de type couleur)
template<>
inline ReflectedComponent reflectComponent<DayNightCycle>(DayNightCycle& c) {
    return { "DayNightCycle", {
        { "hour",               PropertyInfo::Type::Float, &c.hour },
        { "minutesPerGameHour", PropertyInfo::Type::Float, &c.minutesPerGameHour },
        { "paused",             PropertyInfo::Type::Bool,  &c.paused },
        { "dawnHour",           PropertyInfo::Type::Float, &c.dawnHour },
        { "dayHour",            PropertyInfo::Type::Float, &c.dayHour },
        { "duskHour",           PropertyInfo::Type::Float, &c.duskHour },
        { "nightHour",          PropertyInfo::Type::Float, &c.nightHour },
        { "transitionHours",    PropertyInfo::Type::Float, &c.transitionHours },
        { "vignette",           PropertyInfo::Type::Float, &c.vignette },
    } };
}
