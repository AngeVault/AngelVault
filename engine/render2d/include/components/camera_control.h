#pragma once
// Pan / zoom au clavier, à la manette et à la molette (CameraControlSystem).
// Seulement sur la caméra affichée. Pas de pan si elle suit une cible (CameraFollow).
// Les actions sont celles de l'Input Manager (assets/input.cfg).
#include "ecs/reflect.h"
#include <string>

struct CameraControl
{
	std::string actionX = "CameraX";
	std::string actionY = "CameraY";
	std::string actionZoom = "Zoom";           // axe maintenu
	std::string actionZoomWheel = "ZoomWheel"; // crans

	bool panEnabled = true;
	bool zoomEnabled = true;
	float panSpeed = 800.f;      // pixels écran / s
	float zoomSpeed = 1.5f;      // vitesse du zoom maintenu
	float wheelStep = 0.15f;     // +15 % par cran
	float zoomSharpness = 12.f;  // lissage du zoom (0 = immédiat)

	float targetZoom = 0.f;      // zoom visé (0 = le zoom actuel). Un script peut le changer.
};

template<>
inline ReflectedComponent reflectComponent<CameraControl>(CameraControl& c) {
	return { "CameraControl", {
		{ "actionX",         PropertyInfo::Type::String, &c.actionX         },
		{ "actionY",         PropertyInfo::Type::String, &c.actionY         },
		{ "actionZoom",      PropertyInfo::Type::String, &c.actionZoom      },
		{ "actionZoomWheel", PropertyInfo::Type::String, &c.actionZoomWheel },
		{ "panEnabled",      PropertyInfo::Type::Bool,   &c.panEnabled      },
		{ "zoomEnabled",     PropertyInfo::Type::Bool,   &c.zoomEnabled     },
		{ "panSpeed",        PropertyInfo::Type::Float,  &c.panSpeed        },
		{ "zoomSpeed",       PropertyInfo::Type::Float,  &c.zoomSpeed       },
		{ "wheelStep",       PropertyInfo::Type::Float,  &c.wheelStep       },
		{ "zoomSharpness",   PropertyInfo::Type::Float,  &c.zoomSharpness   },
	} };
}
