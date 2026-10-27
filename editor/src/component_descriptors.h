#pragma once
#include "ecs/registry.h"

#include <QString>
#include <QVariant>
#include <functional>
#include <vector>

// Description, côté éditeur, des composants affichés dans l'Inspector.
//
// Chaque champ a un getter et un setter : ça marche aussi pour les champs privés
// (Transform2D::setPosition...) et pour les variantes (forme de Geometry / Collider2D).
// Pour afficher un nouveau composant : ajouter une entrée dans componentDescriptors()
// (component_descriptors.cpp). Les PROPERTY(...) des scripts sont affichées automatiquement.

enum class FieldKind { Float, Int, Bool, Vec2, String, Color, Label };

struct FieldDesc {
    QString name;
    FieldKind kind = FieldKind::Float;
    std::function<QVariant()> get;
    std::function<void(const QVariant&)> set;   // vide = lecture seule
    double min = -1e9, max = 1e9, step = 1.0;
    int decimals = 2;
};

struct ComponentDesc {
    QString name;
    std::function<bool(Registry&, Entity)> has;
    std::function<void(Registry&, Entity)> add;      // vide = ne peut pas être ajouté depuis l'éditeur
    std::function<void(Registry&, Entity)> remove;
    std::function<std::vector<FieldDesc>(Registry&, Entity)> fields;
};

const std::vector<ComponentDesc>& componentDescriptors();
