#pragma once
#include "property.h"
#include <vector>

// Liste de PropertyInfo construite depuis une instance
// Utilisée par l'éditeur pour afficher les champs d'un composant
struct ReflectedComponent {
    std::string                name;     // nom du type ("RigidBody2D")
    std::vector<PropertyInfo>  fields;   // champs éditables
};

// Chaque composant réfléchi implémente cette fonction statique
// L'éditeur l'appelle avec un pointeur vers l'instance concrète
template<typename T>
ReflectedComponent reflectComponent(T& instance);

// Macro pour déclarer un champ réfléchi dans un composant
// Déclare la variable ET l'ajoute à une liste statique via un trick d'init
#define REFLECT_FIELD(T, name, defaultVal) T name = (defaultVal);