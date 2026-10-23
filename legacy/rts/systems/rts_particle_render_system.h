#pragma once

#include "../components/rts_particle_visual.h"
#include <components/particle.h>
#include <components/transform2d.h>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// le rendu des particules est propre au RTS. Le moteur fournit seulement leur
class RtsParticleRenderSystem : public ISystem {
public:
    explicit RtsParticleRenderSystem(sf::RenderTarget& target) : m_target(target) {
        priority = 14;
    }

    void onUpdate(Registry& registry, float) override {
        m_particles.clear();
        registry.view<Transform2D, Particle>([&](Entity, Transform2D& transform, Particle& particle) {
            if (!particle.expired && registry.isValid(particle.emitter)
                && registry.hasComponent<RtsParticleVisual>(particle.emitter)) {
                m_particles.push_back({ &transform, &particle,
                    &registry.getComponent<RtsParticleVisual>(particle.emitter) });
            }
        });

        std::stable_sort(m_particles.begin(), m_particles.end(), [](const Entry& a, const Entry& b) {
            return a.transform->depth < b.transform->depth;
        });

        m_vertices.clear();
        for (const Entry& entry : m_particles)
            appendParticle(entry, m_vertices);
        if (!m_vertices.empty())
            m_target.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles);
    }

private:
    struct Entry {
        const Transform2D* transform;
        const Particle* particle;
        const RtsParticleVisual* visual;
    };

    sf::RenderTarget& m_target;
    std::vector<Entry> m_particles;
    std::vector<sf::Vertex> m_vertices;

    static sf::Color interpolate(sf::Color from, sf::Color to, float t) {
        const auto channel = [t](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(std::clamp(
                std::lround(a + (static_cast<float>(b) - a) * t), 0l, 255l));
        };
        return { channel(from.r, to.r), channel(from.g, to.g),
            channel(from.b, to.b), channel(from.a, to.a) };
    }

    static sf::Color withAlpha(sf::Color color, float factor) {
        color.a = static_cast<std::uint8_t>(std::clamp(color.a * factor, 0.f, 255.f));
        return color;
    }

    // disque, le centre et le bord peuvent avoir des couleurs differentes (halo = bord transparent)
    static void appendDisc(std::vector<sf::Vertex>& vertices, sf::Vector2f center, float radius,
        sf::Color centerColor, sf::Color rimColor, int segments) {
        constexpr float twoPi = 6.28318530718f;
        for (int i = 0; i < segments; ++i) {
            const float a0 = twoPi * static_cast<float>(i) / segments;
            const float a1 = twoPi * static_cast<float>(i + 1) / segments;
            vertices.push_back({ center, centerColor });
            vertices.push_back({ center + sf::Vector2f(std::cos(a0) * radius, std::sin(a0) * radius), rimColor });
            vertices.push_back({ center + sf::Vector2f(std::cos(a1) * radius, std::sin(a1) * radius), rimColor });
        }
    }

    // etincelle : tete ronde devant, queue qui s'efface derriere
    static void appendStreak(std::vector<sf::Vertex>& vertices, sf::Vector2f center, sf::Vector2f velocity,
        float radius, float stretch, sf::Color color) {
        const float speed = std::hypot(velocity.x, velocity.y);
        const sf::Vector2f dir = speed > 0.001f ? velocity / speed : sf::Vector2f(1.f, 0.f);
        const sf::Vector2f side(-dir.y, dir.x);
        const float tail = radius + speed * stretch;
        const sf::Vector2f tip = center + dir * radius;
        const sf::Vector2f left = center + side * radius;
        const sf::Vector2f right = center - side * radius;
        const sf::Vector2f end = center - dir * tail;
        const sf::Color faded = withAlpha(color, 0.f);
        vertices.push_back({ tip, color });
        vertices.push_back({ left, color });
        vertices.push_back({ right, color });
        vertices.push_back({ left, color });
        vertices.push_back({ end, faded });
        vertices.push_back({ right, color });
    }

    static void appendParticle(const Entry& entry, std::vector<sf::Vertex>& vertices) {
        const Particle& particle = *entry.particle;
        const RtsParticleVisual& visual = *entry.visual;
        const float progress = std::clamp(particle.age / std::max(particle.lifetime, 0.001f), 0.f, 1.f);
        const float radius = std::max(0.f,
            visual.startRadius + (visual.endRadius - visual.startRadius) * progress);
        if (radius <= 0.f) return;

        const sf::Vector2f center = entry.transform->getPosition();
        const sf::Color color = interpolate(visual.startColor, visual.endColor, progress);
        const int segments = std::clamp(visual.segments, 3, 32);

        if (visual.glow > 0.f)
            appendDisc(vertices, center, radius * (1.f + visual.glow), withAlpha(color, 0.55f),
                withAlpha(color, 0.f), 12);

        if (visual.stretch > 0.f)
            appendStreak(vertices, center, particle.velocity, radius, visual.stretch, color);
        else
            appendDisc(vertices, center, radius, color, visual.soft ? withAlpha(color, 0.f) : color, segments);
    }
};
