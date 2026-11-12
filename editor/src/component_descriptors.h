#pragma once
#include "ecs/registry.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <functional>
#include <vector>

// Description, côté éditeur, des composants affichés dans l'Inspector.
//
// Chaque champ a un getter et un setter : ça marche aussi pour les champs privés
// (Transform2D::setPosition...) et pour les variantes (forme de Geometry / Collider2D).
// Pour afficher un nouveau composant : ajouter une entrée dans componentDescriptors()
// (component_descriptors.cpp). Les PROPERTY(...) des scripts sont affichées automatiquement.

enum class FieldKind { Float, Int, Bool, Vec2, String, Color, Label, Choice };

struct FieldDesc {
    QString name;
    FieldKind kind = FieldKind::Float;
    std::function<QVariant()> get;
    std::function<void(const QVariant&)> set;   // vide = lecture seule
    double min = -1e9, max = 1e9, step = 1.0;
    int decimals = 2;
    QStringList choices;            // Choice : liste déroulante, la valeur est l'index choisi
    bool rebuildOnChange = false;   // la modification change les champs affichés (ex. : forme de Geometry)
};

struct ComponentDesc {
    QString name;
    std::function<bool(Registry&, Entity)> has;
    std::function<void(Registry&, Entity)> add;      // vide = ne peut pas être ajouté depuis l'éditeur
    std::function<void(Registry&, Entity)> remove;
    std::function<std::vector<FieldDesc>(Registry&, Entity)> fields;
};

const std::vector<ComponentDesc>& componentDescriptors();

// Champs d'un comportement (ses PROPERTY) : une section par comportement dans l'Inspector
struct IBehavior;
std::vector<FieldDesc> behaviorFields(IBehavior* behavior);
