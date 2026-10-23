#pragma once

#include <SFML/Graphics.hpp>
#include "ecs/registry.h"
#include "ecs/system.h"
#include "fog_of_war_system.h"

class WorldRenderTextureBeginSystem : public ISystem {
public:
    WorldRenderTextureBeginSystem(sf::RenderTexture& texture, sf::RenderWindow& window) : m_texture(texture), m_window(window) {
        priority = 4;
    }

    void onUpdate(Registry&, float) override {
        const sf::Vector2u size = m_window.getSize();
        if (size.x == 0 || size.y == 0) return;
        if (m_texture.getSize() != size && !m_texture.resize(size)) return;

        m_texture.setView(m_window.getView());
        m_texture.clear(sf::Color(60, 42, 53));
    }

private:
    sf::RenderTexture& m_texture;
    sf::RenderWindow& m_window;
};

class WorldRenderTextureDisplaySystem : public ISystem {
public:
    WorldRenderTextureDisplaySystem(sf::RenderTexture& texture, sf::RenderWindow& window,
        FogOfWarSystem& fog)
        : m_texture(texture), m_window(window), m_fog(fog) {
        priority = 80;
    }

    void onUpdate(Registry&, float) override {
        m_texture.display();

        const sf::View previousView = m_window.getView();
        const sf::Vector2u windowSize = m_window.getSize();
        const sf::Vector2u textureSize = m_texture.getSize();
        if (windowSize.x == 0 || windowSize.y == 0 || textureSize.x == 0 || textureSize.y == 0)
            return;

        m_window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) })));
        sf::Sprite worldSprite(m_texture.getTexture());
        worldSprite.setScale({ static_cast<float>(windowSize.x) / textureSize.x,  static_cast<float>(windowSize.y) / textureSize.y });
        if (m_fog.shaderReady()) {
            sf::Shader& shader = m_fog.compositeShader();
            const sf::Vector2f viewSize = previousView.getSize();
            const sf::Vector2f worldOrigin = previousView.getCenter() - viewSize * 0.5f;
            shader.setUniform("texture", sf::Shader::CurrentTexture);
            shader.setUniform("visibleMask", m_fog.visibleTexture());
            shader.setUniform("exploredMask", m_fog.exploredTexture());
            shader.setUniform("worldOrigin", sf::Glsl::Vec2(worldOrigin));
            shader.setUniform("viewSize", sf::Glsl::Vec2(viewSize));
            shader.setUniform("worldSize", sf::Glsl::Vec2(m_fog.worldSize()));
            shader.setUniform("screenSize", sf::Glsl::Vec2(
                static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)));
            sf::RenderStates states;
            states.shader = &shader;
            m_window.draw(worldSprite, states);
        }
        else {
            m_window.draw(worldSprite);
        }
        m_window.setView(previousView);
    }

private:
    sf::RenderTexture& m_texture;
    sf::RenderWindow& m_window;
    FogOfWarSystem& m_fog;
};
