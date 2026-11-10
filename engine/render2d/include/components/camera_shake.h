#pragma once
// Tremblement de caméra (CameraShakeSystem), modèle « trauma » :
//   shake.add(0.4f);   // explosion, choc...    ou Camera::shake(registry, 0.4f) (utils/camera_utils.h)
// L'intensité vaut trauma^exponent et retombe de `decay` par seconde. Bruit lissé : pas de saccades.
// Ne touche pas au Transform2D : seulement à la vue de la frame.
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

#include <algorithm>

struct CameraShake
{
	float trauma = 0.f;                    // 0..1
	float decay = 1.5f;                    // trauma perdu par seconde
	sf::Vector2f maxOffset{ 24.f, 24.f };  // décalage max (unités monde)
	float maxRotation = 3.f;               // degrés
	float frequency = 18.f;                // Hz
	float exponent = 2.f;                  // 2 = petits chocs discrets, gros chocs forts

	float time = 0.f;                      // état du bruit

	void add(float amount) { trauma = std::clamp(trauma + amount, 0.f, 1.f); }
};

template<>
inline ReflectedComponent reflectComponent<CameraShake>(CameraShake& c) {
	return { "CameraShake", {
		{ "trauma",      PropertyInfo::Type::Float, &c.trauma      },
		{ "decay",       PropertyInfo::Type::Float, &c.decay       },
		{ "maxOffset",   PropertyInfo::Type::Vec2f, &c.maxOffset   },
		{ "maxRotation", PropertyInfo::Type::Float, &c.maxRotation },
		{ "frequency",   PropertyInfo::Type::Float, &c.frequency   },
		{ "exponent",    PropertyInfo::Type::Float, &c.exponent    },
	} };
}
