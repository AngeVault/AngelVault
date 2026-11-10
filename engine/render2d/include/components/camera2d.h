#pragma once
// Caméra 2D : ce que voit le jeu. Position = Transform2D de l'entité.
// Le reste est fait par des composants optionnels, chacun avec son système :
//   CameraFollow  suivre une entité          CameraBounds  rester dans une zone
//   CameraControl pan / zoom à la manette    CameraShake   tremblement
// Plusieurs caméras : la plus prioritaire (active) est affichée, avec un fondu de `blendTime` s.
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

#include <algorithm>

struct Camera2d
{
	float zoom = 1.f;          // 2 = deux fois plus près
	float minZoom = 0.25f;
	float maxZoom = 4.f;
	float rotation = 0.f;      // degrés
	int priority = 0;          // la plus grande est affichée
	bool active = true;        // false = jamais affichée
	float blendTime = 0.f;     // fondu (s) quand elle devient la caméra affichée

	// Effets de la frame (CameraShake...) : ajoutés à la vue puis remis à zéro par le CameraSystem
	sf::Vector2f effectOffset{ 0.f, 0.f };
	float effectRotation = 0.f;
	float effectZoom = 1.f;

	void setZoom(float z) { zoom = std::clamp(z, minZoom, maxZoom); }
};

template<>
inline ReflectedComponent reflectComponent<Camera2d>(Camera2d& c) {
	return { "Camera2d", {
		{ "zoom",      PropertyInfo::Type::Float, &c.zoom      },
		{ "minZoom",   PropertyInfo::Type::Float, &c.minZoom   },
		{ "maxZoom",   PropertyInfo::Type::Float, &c.maxZoom   },
		{ "rotation",  PropertyInfo::Type::Float, &c.rotation  },
		{ "priority",  PropertyInfo::Type::Int,   &c.priority  },
		{ "active",    PropertyInfo::Type::Bool,  &c.active    },
		{ "blendTime", PropertyInfo::Type::Float, &c.blendTime },
	} };
}
