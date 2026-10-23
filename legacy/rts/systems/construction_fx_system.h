#pragma once

#include "../components/buildingComp.h"
#include "../utils/rts_fx.h"
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

// particules de construction : poussiere au premier coup de pioche, poussiere + etincelles + copeaux
// tant que des batisseurs travaillent, et une petite explosion de joie quand c'est fini
class ConstructionFxSystem : public ISystem {
public:
    ConstructionFxSystem() { priority = 2; } // avant ParticleSystem (3)

    void onUpdate(Registry& registry, float dt) override {
        if (dt <= 0.f) return; // pause : rien ne bouge
        ++m_frame;
        m_events.clear();

        registry.view<ECS::Building, Transform2D>([&](Entity e, ECS::Building& building, Transform2D& transform) {
            if (building.pv <= 0) return;
            State& state = m_states[key(e)];
            state.seen = m_frame;
            const bool underConstruction = building.constructionProgress < 1.f;
            const sf::Vector2f position = transform.getPosition();
            // la zone de travail grandit avec le batiment
            const float growth = 0.35f + 0.65f * building.constructionProgress;
            const sf::Vector2f area(building.halfSize.x * 2.f * growth, building.halfSize.y * 2.f * growth);

            if (!state.known) {
                state.known = true;
                if (underConstruction) m_events.push_back({ Kind::GroundBreak, position, area, 0 });
            }
            else if (state.underConstruction && !underConstruction) {
                m_events.push_back({ Kind::Complete, position, building.halfSize * 2.f, 0 });
            }
            state.underConstruction = underConstruction;

            if (underConstruction && building.assignedBuilderCount > 0) {
                // plus il y a de batisseurs, plus ca tape vite
                const int builders = std::min(building.assignedBuilderCount, 5);
                const float interval = 0.16f / (0.6f + 0.4f * static_cast<float>(builders));
                state.timer += dt;
                int ticks = 0;
                while (state.timer >= interval && ticks < 3) {
                    state.timer -= interval;
                    ++ticks;
                    m_events.push_back({ Kind::Work, position, area, state.tick++ });
                }
                state.timer = std::min(state.timer, interval); // pas de rattrapage apres un gros dt
            }
            else {
                state.timer = 0.f;
            }
        });

        for (auto it = m_states.begin(); it != m_states.end();) {
            if (it->second.seen != m_frame) it = m_states.erase(it);
            else ++it;
        }

        for (const Event& event : m_events) spawn(registry, event);
    }

private:
    enum class Kind { GroundBreak, Work, Complete };

    struct Event {
        Kind kind;
        sf::Vector2f position;
        sf::Vector2f area; // taille de la zone ou apparaissent les particules
        int tick;
    };

    struct State {
        bool known = false;
        bool underConstruction = false;
        float timer = 0.f;
        int tick = 0;
        std::uint64_t seen = 0;
    };

    std::unordered_map<std::uint64_t, State> m_states;
    std::vector<Event> m_events;
    std::uint64_t m_frame = 0;

    static std::uint64_t key(Entity e) {
        return (static_cast<std::uint64_t>(e.generation) << 32) | e.index;
    }

    static void spawn(Registry& registry, const Event& event) {
        using namespace Rts::Fx;
        const sf::Vector2f p = event.position;
        switch (event.kind) {
        case Kind::GroundBreak:
            // le sol qui se souleve
            burst(registry, p, inRect(emitter(15.f, 55.f, 0.5f, 0.9f, { 0.f, -12.f }), event.area),
                visual({ 175, 155, 125, 200 }, { 120, 105, 85, 0 }, 6.f, 20.f, 0.f, 0.f, 12, true), 8);
            break;

        case Kind::Work: {
            // poussiere qui monte
            burst(registry, p, inRect(emitter(6.f, 26.f, 0.5f, 0.9f, { 0.f, -14.f }), event.area),
                visual({ 205, 190, 160, 150 }, { 150, 135, 115, 0 }, 4.f, 13.f, 0.f, 0.f, 10, true), 2);
            // etincelles de marteau
            burst(registry, p, inRect(upward(emitter(60.f, 150.f, 0.18f, 0.38f, { 0.f, 300.f }), 60.f), event.area * 0.7f),
                visual({ 255, 240, 170, 255 }, { 255, 150, 50, 0 }, 1.8f, 0.4f, 0.03f), 3);
            // copeaux de bois, un coup sur deux
            if (event.tick % 2 == 0)
                burst(registry, p, inRect(upward(emitter(40.f, 110.f, 0.3f, 0.55f, { 0.f, 320.f }), 70.f), event.area * 0.8f),
                    visual({ 190, 145, 90, 255 }, { 105, 78, 50, 0 }, 2.2f, 0.8f, 0.02f), 2);
            break;
        }

        case Kind::Complete: {
            const float size = std::max(event.area.x, event.area.y);
            // flash dore
            burst(registry, p, emitter(0.f, 0.f, 0.2f, 0.26f, {}),
                visual({ 255, 245, 200, 255 }, { 255, 200, 90, 0 }, size * 0.32f, size * 0.06f, 0.f, 0.9f, 14, true), 1);
            // nuage de poussiere tout autour
            burst(registry, p, inRect(emitter(25.f, 80.f, 0.6f, 1.1f, { 0.f, -10.f }), event.area),
                visual({ 195, 180, 150, 190 }, { 130, 115, 95, 0 }, 7.f, 24.f, 0.f, 0.f, 12, true), 12);
            // etincelles dorees qui montent
            burst(registry, p, inRect(upward(emitter(60.f, 190.f, 0.5f, 0.95f, { 0.f, 90.f }), 50.f), event.area * 0.8f),
                visual({ 255, 245, 190, 255 }, { 255, 190, 70, 0 }, 2.6f, 0.6f, 0.04f, 0.5f), 18);
            break;
        }
        }
    }
};
