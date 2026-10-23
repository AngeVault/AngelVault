#pragma once
#include <SFML/Graphics.hpp>


struct Camera2d
{
	// state
	sf::Vector2f position{ 0.f ,0.f };
	float zoom = 1.f;
	float minZoom = 0.25f;
	float maxZoom = 4.f;

	// speed 
	float panSpeedKeyboard = 100.f;
	float panSpeedGamepad = 100.f;
	float zoomStep = 0.2f;
	float zoomLerpSpeed = 12.f;

	// border
	sf::Vector2f worldMin{ 0.f , 0.f };
	sf::Vector2f worldMax{ 0.f , 0.f };

	float targetZoom = 1.f;

	// helper 
	void setWorldBorder(sf::Vector2f min, sf::Vector2f max)
	{
		worldMin = min;
		worldMax = max;
	}

	//
	sf::View makeView(sf::Vector2u windowSize) const {
		sf::Vector2f half =
		{
			windowSize.x * 0.5f / zoom,
			windowSize.y * 0.5f / zoom
		};
		return sf::View(sf::FloatRect(
			{ position.x - half.x, position.y - half.y },
			{ half.x * 2.f, half.y * 2.f }
		));
	};

	void clampPosition(sf::Vector2u windowSize)
	{
		if (worldMax.x <= worldMin.x || worldMax.y <= worldMin.y) return;

		float halfW = (windowSize.x * 0.5f) /zoom;
		float halfH = (windowSize.y * 0.5f) /zoom;

		float minX = worldMin.x + halfW;
		float maxX = worldMax.x - halfW;
		float minY = worldMin.y + halfH;
		float maxY = worldMax.y - halfH;

		if (minX > maxX) position.x = (worldMin.x + worldMax.x) * 0.5f;
		else position.x = std::clamp(position.x, minX, maxX);
		if (minY > maxY) position.y = (worldMin.y + worldMax.y) * 0.5f;
		else position.y = std::clamp(position.y, minY, maxY);
	};

};
