//
// Created by p.pagliazzo on 18/11/2026.
//

#pragma once
#include <iostream>
#include <SFML/System/Vector2.hpp>
#include "ecs/reflect.h"
#include "iostream"

struct SteeringFleeComponent
{
    struct Cible
    {
        Cible(sf::Vector2f _position, bool _distanfceAdaptable, float _distanfceAdaptableDebut, float _angle)
        {
            id=idNext; idNext++;
            position = _position;
            distanfceAdaptable = _distanfceAdaptable;
            distanfceAdaptableDebut = _distanfceAdaptableDebut;
            angle = _angle;
        }
        sf::Vector2f position;
        unsigned id;
        unsigned idNext = 0;
        Cible* next = nullptr;
        float angle = 0.0f;
        bool distanfceAdaptable;
        float distanfceAdaptableDebut;
    };
    bool isActif = true;
    Cible* listeCible = nullptr;
    unsigned addCible(sf::Vector2f _position, bool _distanfceAdaptable, float _distanfceAdaptableDebut, float _angle)
    {
        if (listeCible == nullptr)
        {
            listeCible = new Cible(_position, _distanfceAdaptable, _distanfceAdaptableDebut, _angle);
            return listeCible -> id;
        }
        else
        {
            Cible* temp = listeCible;
            while (temp->next != nullptr)
            {
                temp = temp->next;
            }
            temp->next = new Cible(_position, _distanfceAdaptable, _distanfceAdaptableDebut, _angle);
            return temp -> next -> id;
        }

    }
    bool substractCible(unsigned id)
    {
        Cible* temp = listeCible;
        if (listeCible == nullptr)
        {
            std::cerr << "on peut pas enlever un cible si la liste est vide" << std::endl;
            return true;
        }
        if (temp->id == id)
        {
            Cible* suprtemp = listeCible;
            listeCible = listeCible -> next;
            delete suprtemp;
            return false;
        }
        while (temp->next != nullptr)
        {
            if (temp->next->id == id)
            {
                Cible* suprtemp = temp->next;
                temp->next = suprtemp->next;
                delete suprtemp;
                return false;
            }
                temp = temp->next;
        }
        return true;
    }
};
template<>
inline ReflectedComponent reflectComponent<SteeringFleeComponent>(SteeringFleeComponent& rb) {
    return { "SteeringComponent", {
                { "isActif", PropertyInfo::Type::Bool, &rb.isActif }
    } };
}