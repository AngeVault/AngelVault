#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Angle.hpp>
#include "ecs/reflect.h"

struct Transform2D {
private:
    sf::Vector2f m_position = { 0.0f, 0.0f };
    sf::Angle    m_rotation = sf::degrees(0.0f);
    sf::Vector2f m_scale = { 1.0f, 1.0f };

    mutable sf::Transform m_localMatrix;
    mutable bool          m_matrixDirty = true;

public:
    float depth = 0.0f;

    void setPosition(const sf::Vector2f& pos) { m_position = pos; m_matrixDirty = true; }
    void setRotation(sf::Angle angle) { m_rotation = angle; m_matrixDirty = true; }
    void setScale(const sf::Vector2f& scale) { m_scale = scale; m_matrixDirty = true; }

    // position += offset
    void move(const sf::Vector2f& offset) { m_position += offset; m_matrixDirty = true; }
    // rotation += angle
    void rotate(sf::Angle angle) { m_rotation += angle; m_matrixDirty = true; }

    // getter
    const sf::Vector2f& getPosition() const { return m_position; }
    sf::Angle           getRotation() const { return m_rotation; }
    const sf::Vector2f& getScale()    const { return m_scale; }

    // Matrice
    const sf::Transform& getLocalMatrix() const {
        if (m_matrixDirty) {
            m_localMatrix = sf::Transform::Identity;
            m_localMatrix.translate(m_position);
            m_localMatrix.rotate(m_rotation);
            m_localMatrix.scale(m_scale);
            m_matrixDirty = false; // Le cache est à jour !
        }
        return m_localMatrix;
    }
};

	// Reflection
	// Spécialisation de reflectComponent — dit à l'éditeur quoi afficher
template<>
inline ReflectedComponent reflectComponent<Transform2D>(Transform2D& t) {
    return { "Transform2D", {
        { "depth", PropertyInfo::Type::Float, &t.depth },
            
    } };
}