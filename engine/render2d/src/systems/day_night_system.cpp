#include "systems/day_night_system.h"

#include <iostream>

namespace {
// Copié dans bin/shaders/ au build (engine/render2d/shaders/)
constexpr const char* kShaderPath = "shaders/daynight.frag";
} // namespace

DayNightSystem::DayNightSystem(sf::RenderTarget& target) : m_target(target) {
    priority = 11;   // après le rendu (10), avant les dessins de debug
}

void DayNightSystem::loadShader() {
    m_shaderTried = true;
    if (!sf::Shader::isAvailable()) return;
    sf::Shader shader;
    if (!shader.loadFromFile(kShaderPath, sf::Shader::Type::Fragment)) {
        std::cerr << "[DayNight] " << kShaderPath << " introuvable ou invalide : teinte sans vignettage" << std::endl;
        return;
    }
    m_shader.emplace(std::move(shader));
}

void DayNightSystem::onUpdate(Registry& registry, float dt) {
    DayNightCycle* cycle = nullptr;
    registry.view<DayNightCycle>([&](Entity, DayNightCycle& c) { if (!cycle) cycle = &c; });
    if (!cycle) return;
    cycle->advance(dt);

    const sf::Color ambient = cycle->ambient();
    if (ambient == sf::Color::White) return;   // plein jour : rien à dessiner
    if (!m_shaderTried) loadShader();

    // Quad 0..1 sur la zone de la caméra (pas sur les bandes noires autour)
    const sf::View previous = m_target.getView();
    sf::View unit(sf::FloatRect({ 0.f, 0.f }, { 1.f, 1.f }));
    unit.setViewport(previous.getViewport());
    m_target.setView(unit);

    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    const sf::Vector2f corners[] = { { 0.f, 0.f }, { 1.f, 0.f }, { 0.f, 1.f }, { 1.f, 1.f } };
    for (std::size_t i = 0; i < 4; ++i) quad[i] = sf::Vertex{ corners[i], ambient, corners[i] };

    sf::RenderStates states(sf::BlendMultiply);
    if (m_shader) {
        m_shader->setUniform("ambient", sf::Glsl::Vec3(ambient.r / 255.f, ambient.g / 255.f, ambient.b / 255.f));
        m_shader->setUniform("vignette", std::clamp(cycle->vignette, 0.f, 1.f) * cycle->darkness());
        states.shader = &*m_shader;
    }
    m_target.draw(quad, states);
    m_target.setView(previous);
}
