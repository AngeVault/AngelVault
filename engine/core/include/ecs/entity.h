#pragma once
#include <cstdint>

struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;                  // évite les accès à des entités mortes

    bool operator==(const Entity& o) const {
        return index == o.index && generation == o.generation;
    }
};

inline constexpr Entity NULL_ENTITY = { UINT32_MAX, 0 };