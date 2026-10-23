#pragma once
#include "ecs/reflect.h"
#include <SFML/System/Vector2.hpp>

struct RigidBody2D {
    float mass = 1.0f;
    float linearDrag = 0.05f;
    float friction = 0.4f;
    bool isStatic = false;
    bool useGravity = true;
    bool isGrounded = false;

    sf::Vector2f velocity = {};
    sf::Vector2f forceAccumulator = {};

    float inertia = 0.f;
	float angularVelocity = 0.4f;
	float torque = 0.f;
	bool fixedRotation = false;

    void applyForce(sf::Vector2f f) { if (!isStatic) forceAccumulator += f; }
    void applyImpulse(sf::Vector2f i) { if (!isStatic) velocity += i / mass; }
    float inverseMass() const { return isStatic ? 0.f : 1.f / mass; }

    void applyTorque(float i) { if (!isStatic && !fixedRotation) torque += i; }
    float inverseInertia() const { return (isStatic || fixedRotation) ? 0.f : 1.f / inertia; }
}; 

// La spécialisation est DEHORS, au niveau global
template<>
inline ReflectedComponent reflectComponent<RigidBody2D>(RigidBody2D& rb) {
    return { "RigidBody2D", {
        { "mass",       PropertyInfo::Type::Float, &rb.mass       },
        { "linearDrag", PropertyInfo::Type::Float, &rb.linearDrag },
        { "friction",   PropertyInfo::Type::Float, &rb.friction   },
        { "isStatic",   PropertyInfo::Type::Bool,  &rb.isStatic   },
        { "useGravity", PropertyInfo::Type::Bool,  &rb.useGravity },
        { "inertia",    PropertyInfo::Type::Float, &rb.inertia    },
        { "fixedRotation", PropertyInfo::Type::Bool, &rb.fixedRotation}
    } };
}