#pragma once
// Suit une entité (CameraFollowSystem). Marche sur toute entité avec Transform2D, pas seulement une caméra.
#include "ecs/entity.h"
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

struct CameraFollow
{
	Entity target = NULL_ENTITY;
	sf::Vector2f offset{ 0.f, 0.f };     // décalage par rapport à la cible
	float smoothTime = 0.15f;            // s pour rattraper la cible (0 = collée)
	float maxSpeed = 0.f;                // 0 = illimitée
	sf::Vector2f deadZone{ 0.f, 0.f };   // demi-taille : la cible bouge dedans sans faire bouger la caméra
	float lookAhead = 0.f;               // avance dans le sens de la vitesse (RigidBody2D)
	float lookAheadTime = 0.3f;          // lissage de l'avance (s)
	bool snapOnStart = true;             // se place sur la cible à la première frame

	// état
	sf::Vector2f velocity{ 0.f, 0.f };
	sf::Vector2f lookAheadOffset{ 0.f, 0.f };
	sf::Vector2f lookAheadVelocity{ 0.f, 0.f };
	bool started = false;

	void setTarget(Entity e) { target = e; started = false; }
};

// Champs réglables (target est sauvegardé à part : c'est une Entity)
template<>
inline ReflectedComponent reflectComponent<CameraFollow>(CameraFollow& c) {
	return { "CameraFollow", {
		{ "offset",        PropertyInfo::Type::Vec2f, &c.offset        },
		{ "smoothTime",    PropertyInfo::Type::Float, &c.smoothTime    },
		{ "maxSpeed",      PropertyInfo::Type::Float, &c.maxSpeed      },
		{ "deadZone",      PropertyInfo::Type::Vec2f, &c.deadZone      },
		{ "lookAhead",     PropertyInfo::Type::Float, &c.lookAhead     },
		{ "lookAheadTime", PropertyInfo::Type::Float, &c.lookAheadTime },
		{ "snapOnStart",   PropertyInfo::Type::Bool,  &c.snapOnStart   },
	} };
}
