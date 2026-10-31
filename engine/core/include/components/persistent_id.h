#pragma once
// Identifiant stable d'une entité, utilisé UNIQUEMENT pour la sauvegarde.
//
// Entity { index, generation } est un numéro de place dans le Registry : il change d'une session
// à l'autre (ordre de création, cases recyclées). PersistentId, lui, est donné une fois à la
// création de l'entité et écrit dans le fichier .scene : les liens entre entités
// (ex. Camera2d::targetLock) sont sauvegardés avec lui, puis retraduits au chargement.
//
// 0 = pas d'identifiant. Les entités créées en jeu (balles, particules) n'en ont pas besoin.
#include <chrono>
#include <cstdint>
#include <random>

struct PersistentId {
    uint64_t value = 0;
};

// Nouvel identifiant aléatoire 64 bits (jamais 0) : pas de collision en pratique,
// même en copiant des entités d'une scène à l'autre.
inline uint64_t generatePersistentId() {
    static std::mt19937_64 rng([] {
        std::random_device rd;
        const uint64_t time = uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return (uint64_t(rd()) << 32) ^ uint64_t(rd()) ^ time;
    }());
    uint64_t id = 0;
    while (id == 0) id = rng();
    return id;
}
