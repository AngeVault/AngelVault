#pragma once 
#include <ecs/system.h>
#include <SFML/Graphics.hpp>
#include "../components/camera2d.h"
#include "../RtsInput_state.h"

class CameraSysteme : public ISystem {
public:
	CameraSysteme(sf::RenderWindow& window, InputState& input, Camera2d& camera) : m_window(window), m_input(input), m_camera(camera)
	{
		priority = -10;
	};

	void onUpdate(Registry& registry, float dt) override
	{
		const sf::Vector2u viewportSize = m_window.getSize();
		const float worldWidth = m_camera.worldMax.x - m_camera.worldMin.x;
		const float worldHeight = m_camera.worldMax.y - m_camera.worldMin.y;
		if (viewportSize.x > 0 && viewportSize.y > 0 && worldWidth > 0.f && worldHeight > 0.f) {
			// Empêche le zoom arrière d'élargir la vue au-delà de la carte.
			const float fitZoom = std::max(
				static_cast<float>(viewportSize.x) / worldWidth,
				static_cast<float>(viewportSize.y) / worldHeight);
			m_camera.minZoom = std::min(fitZoom, m_camera.maxZoom);
			m_camera.zoom = std::max(m_camera.zoom, m_camera.minZoom);
			m_camera.targetZoom = std::clamp(m_camera.targetZoom,
				m_camera.minZoom, m_camera.maxZoom);
		}

		if (m_input.minimapPressed || m_input.minimapDown)
			m_camera.position = m_input.minimapWorldPosition;

		if (m_input.middleMousePressed)
			m_lastMouse = m_input.mouseScreenPos; 

		if (m_input.middleMouseDown)
		{
			sf::Vector2f delta = m_input.mouseScreenPos - m_lastMouse;
			m_camera.position -= delta / m_camera.zoom;
			m_lastMouse = m_input.mouseScreenPos;
		}

		sf::Vector2f pan = { 0.f , 0.f };
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
			pan.y = -1;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
			pan.x = -1;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
			pan.y = 1;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
			pan.x = 1;

		float len = std::hypot(pan.x, pan.y);
		if (len > 0.f) pan /= len;
		m_camera.position += pan * m_camera.panSpeedKeyboard * dt;
		m_camera.position += m_input.cameraPan * m_camera.panSpeedGamepad * dt;

		if (m_input.zoomDelta != 0.f) {
			m_camera.targetZoom += m_input.zoomDelta * m_camera.zoomStep;
		}
		m_camera.targetZoom = std::clamp(m_camera.targetZoom, m_camera.minZoom, m_camera.maxZoom);

		float oldZoom = m_camera.zoom;

		float t = 1.f - std::exp(-m_camera.zoomLerpSpeed * dt);
		m_camera.zoom += (m_camera.targetZoom - m_camera.zoom) * t;

		if (std::abs(m_camera.zoom - oldZoom) > 0.0001f) {
			sf::Vector2i mousePos = sf::Mouse::getPosition(m_window);
			sf::Vector2f center(m_window.getSize().x * 0.5f, m_window.getSize().y * 0.5f);

			sf::Vector2f mouseOffset(mousePos.x - center.x, mousePos.y - center.y);
			m_camera.position += mouseOffset * (1.f / oldZoom - 1.f / m_camera.zoom);
		}

		m_camera.clampPosition(m_window.getSize());

		m_window.setView(m_camera.makeView(m_window.getSize()));
	}


private:
	sf::RenderWindow& m_window;
	InputState& m_input;
	Camera2d& m_camera;
	sf::Vector2f m_lastMouse{ 0.f, 0.f }; 
};
