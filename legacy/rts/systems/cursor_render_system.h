#pragma once
#include "ecs/system.h"
#include <SFML/Graphics.hpp>
#include "../RtsInput_state.h"
#include "../utils/loose_file_loader.h"

class CursorRenderSystem : public ISystem {
public:
    CursorRenderSystem(sf::RenderWindow& window, LooseFileLoader& textureLoader, InputState& input)
        : m_window(window), m_textureLoader(textureLoader), m_input(input) {
        priority = 100; 
    }

    void onUpdate(Registry&, float) override {
        const sf::Vector2f position = m_input.gamepadConnected  ? m_input.mouseScreenPos  : sf::Vector2f(sf::Mouse::getPosition(m_window));
        draw(m_window, m_textureLoader.loadTexture("cursor"), position);
    }

    // dessine le curseur par dessus les menus, sinon ils le cachent
    static void draw(sf::RenderWindow& window, const sf::Texture& texture, sf::Vector2f position) {
        const sf::View previousView = window.getView();
        const sf::Vector2u size = window.getSize();
        window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, { (float)size.x, (float)size.y })));

        sf::Sprite cursorSprite{ texture };
        cursorSprite.setPosition(position);
        cursorSprite.setTextureRect({ { 0, 0 }, { 145, 145 } });
        cursorSprite.setScale({ 0.25f, 0.25f });

        window.draw(cursorSprite);
        window.setView(previousView);
    }

private:
    sf::RenderWindow& m_window;
    LooseFileLoader& m_textureLoader;
    InputState& m_input;
};
