#pragma once
#include "ecs/behavior.h"
#include "ecs/registry.h"
#include "components/transform2d.h"

class Rotation : public IBehavior{
	void onUpdate(Entity self, Registry& registry, float dt) override
	{
		auto& t = registry.getComponent<Transform2D>(self);
		float rotationSpeed = 45.0f;
		t.setRotation(t.getRotation() + sf::degrees(rotationSpeed * dt));

		return;
	}
};