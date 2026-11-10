#pragma once
// Garde la vue de la caméra dans une zone du monde (CameraBoundsSystem).
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

struct CameraBounds
{
	sf::Vector2f min{ 0.f, 0.f };
	sf::Vector2f max{ 0.f, 0.f };   // max <= min : pas de limite
	bool limitZoom = true;          // pas de dézoom qui montrerait l'extérieur

	bool valid() const { return max.x > min.x && max.y > min.y; }
	void set(sf::Vector2f mn, sf::Vector2f mx) { min = mn; max = mx; }
};

template<>
inline ReflectedComponent reflectComponent<CameraBounds>(CameraBounds& c) {
	return { "CameraBounds", {
		{ "min",       PropertyInfo::Type::Vec2f, &c.min       },
		{ "max",       PropertyInfo::Type::Vec2f, &c.max       },
		{ "limitZoom", PropertyInfo::Type::Bool,  &c.limitZoom },
	} };
}
