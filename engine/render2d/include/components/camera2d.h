#pragma once
#include <SFML/Graphics.hpp>


struct Camera2d
{
	// state
	float zoom = 1.f;
	float minZoom = 0.25f;
	float maxZoom = 4.f;
	bool lock = false;

	// speed 
	float panSpeedKeyboard = 1000.f;
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


};

template<>
inline ReflectedComponent reflectComponent<Camera2d>(Camera2d& rb) {
	return { "Camera2d", {
	        { "zoom",       PropertyInfo::Type::Float, &rb.zoom       },
			{ "minZoom", PropertyInfo::Type::Float, &rb.minZoom },
			{ "maxZoom",   PropertyInfo::Type::Float, &rb.maxZoom   },
			{ "lock",   PropertyInfo::Type::Bool,  &rb.lock   },
			{ "panSpeedKeyboard",    PropertyInfo::Type::Float, &rb.panSpeedKeyboard    },
			{ "panSpeedGamepad",    PropertyInfo::Type::Float, &rb.panSpeedGamepad    },
			{ "zoomStep",    PropertyInfo::Type::Float, &rb.zoomStep    },
			{ "zoomLerpSpeed",    PropertyInfo::Type::Float, &rb.zoomLerpSpeed    },
			{ "targetZoom",    PropertyInfo::Type::Float, &rb.targetZoom    },
	} };
}