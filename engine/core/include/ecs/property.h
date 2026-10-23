#pragma once
#include <string>
#include <vector>
#include <type_traits>
#include <SFML/System/Vector2.hpp>

struct PropertyInfo {
    std::string name;

    enum class Type { Float, Int, Bool, Vec2f, String} type;

    void* ptr; 

    template<typename T>
    T& as() { return *static_cast<T*>(ptr); }

    template<typename T>
    const T& as() const { return *static_cast<const T*>(ptr); }

    // Déduit le type enum depuis le type C++
    template<typename T>
    static Type typeFor() {
        if constexpr (std::is_same_v<T, float>)        return Type::Float;
        else if constexpr (std::is_same_v<T, int>)          return Type::Int;
        else if constexpr (std::is_same_v<T, bool>)         return Type::Bool;
        else if constexpr (std::is_same_v<T, sf::Vector2f>) return Type::Vec2f;
		else if constexpr (std::is_same_v<T, std::string>) return Type::String;
        else static_assert(sizeof(T) == 0,
            "Type non supporte par PROPERTY — ajoute-le dans PropertyInfo::typeFor()");
    }
};


#define PROPERTY(T, name, defaultVal)                                         \
    T name = (defaultVal);                                                    \
    struct _PropReg_##name {                                                  \
        _PropReg_##name(std::vector<PropertyInfo>& props, T& val) {           \
            props.push_back({ #name, PropertyInfo::typeFor<T>(), &val });     \
        }                                                                     \
    } _propReg_##name { m_properties, name };