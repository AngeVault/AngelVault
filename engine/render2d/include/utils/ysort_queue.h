#pragma once
// File de tri en Y partagée entre le rendu des tilemaps et celui des entités.
//
//  - TilemapRenderSystem (priorité 5) y dépose les tiles des tilesets « tri en Y » (arbres, murs...)
//    au lieu de les dessiner avec le sol.
//  - RenderSystem (priorité 10) les mélange à ses entités : tri par Geometry::layer, puis par Y (le point
//    de tri d'une entité = le bas de sa forme, ses « pieds »), puis les dessine et vide la file.
//    Les tiles sont triées avec les entités de Geometry::layer == kEntityLayer (celui des personnages).
//
// Une file par cible de rendu (fenêtre du jeu, vue de l'éditeur...) : les deux systèmes qui dessinent
// sur la même cible se retrouvent sans rien brancher. Si aucun RenderSystem ne dessine sur cette cible,
// la tilemap dessine ses tiles tout de suite (hasConsumer == false).
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <unordered_map>
#include <vector>

struct YSortQueue {
    static constexpr int kEntityLayer = 1;   // Geometry::layer des personnages (Player du RPG)

    struct Item {
        float y = 0.f;        // point de tri (monde)
        sf::Sprite sprite;
    };
    std::vector<Item> items;
    int consumers = 0;        // RenderSystem qui vident cette file
    bool hasConsumer() const { return consumers > 0; }

    static YSortQueue& of(const sf::RenderTarget& target) {
        static std::unordered_map<const sf::RenderTarget*, YSortQueue> queues;
        return queues[&target];
    }
};
