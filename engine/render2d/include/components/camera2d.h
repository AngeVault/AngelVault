#pragma once

#include "ecs/reflect.h"


struct Camera2d
{
	// state
	float zoom = 1.f;

	// speed
	float zoomStep = 0.5f;
	float zoomLerpSpeed = 1.f;

	float targetZoom = 1.f;

	void setTargetZoom(float _targetZoom)
	{
		targetZoom = _targetZoom;
	};

};

template<>
inline ReflectedComponent reflectComponent<Camera2d>(Camera2d& rb) {
	return { "Camera2d", {
	        { "zoom",       PropertyInfo::Type::Float, &rb.zoom       },
			{ "zoomStep",    PropertyInfo::Type::Float, &rb.targetZoom    },
			{ "zoomLerpSpeed",    PropertyInfo::Type::Float, &rb.targetZoom    },
			{ "targetZoom",    PropertyInfo::Type::Float, &rb.targetZoom    },
	} };
}