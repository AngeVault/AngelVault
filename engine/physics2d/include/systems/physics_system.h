#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/rigid_body2d.h"
#include "components/transform2d.h"
#include <SFML/System/Vector2.hpp>
#include <cmath>

class PhysicsSystem : public ISystem {
public:

    sf::Vector2f gravity = { 0.f, 980.f }; // pixels/s² (9.8 m/s² * 100 pixels/m)
    float        terminalVelocity = 200000.f;       // vitesse max en chute libre

    PhysicsSystem() { priority = 0; } // s'exécute en premier

    void onUpdate(Registry& registry, float dt) override {



        dt = std::min(dt, 0.05f); 

        registry.view<RigidBody2D, Transform2D>(
            [&](Entity e, RigidBody2D& rb, Transform2D& t) {

                if (rb.isStatic) return;
                rb.isGrounded = false; 

                // Appliquer la gravité
                // F_gravité = masse * g  (Newton)
                if (rb.useGravity)
                    rb.applyForce(rb.mass * gravity);

                //  Calculer l'accélération
                // a = F_total / masse  (Newton : F = m*a donc a = F/m)
                sf::Vector2f acceleration = rb.forceAccumulator * rb.inverseMass();

                // Intégration de la vitesse (Euler semi-implicite)
                // v_new = v + a * dt
                // Semi-implicite = on applique le drag AVANT de mettre à jour
                // la position → plus stable numériquement qu'Euler explicite pur
                rb.velocity += acceleration * dt;

                // Drag (résistance de l'air)
                // F_drag = -drag * vitesse
                // Plus le drag est grand, plus l'objet ralentit vite
                // Ex: une plume a un drag élevé, une balle de métal faible
                float dragFactor = 1.0f - (rb.linearDrag * dt);
                rb.velocity *= std::max(0.f, dragFactor); // clamp à 0 pour éviter d'inverser

                //  Clamp vitesse terminale (chute libre)
                // En réalité, un objet ne tombe pas indéfiniment de plus en plus vite
                // car l'air résiste — on simule ça avec une vitesse max en Y
                if (rb.velocity.y > terminalVelocity)
                    rb.velocity.y = terminalVelocity;

                //  Intégration de la position
                // pos_new = pos + v * dt
                t.move(rb.velocity * dt);

                // Reset des forces
                // Les forces se recalculent à chaque frame (gravité etc.)
                // sans reset, elles s'accumulent indéfiniment
                rb.forceAccumulator = { 0.f, 0.f };
            });
    }
};