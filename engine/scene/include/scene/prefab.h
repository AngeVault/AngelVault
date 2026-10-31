#pragma once
// Prefabs (.prefab, JSON) : une ou plusieurs entités enregistrées pour être recréées à volonté.
//
//     // Éditeur : clic droit sur une entité > Créer un prefab, ou la glisser dans le panneau Project.
//     savePrefab(registry, prefabGroup(registry, player), "Player", "prefabs/Player.prefab");
//
//     // Jeu (dans un behavior) : faire apparaître un ennemi à une position
//     auto r = instantiatePrefabFile(registry, "prefabs/Enemy.prefab", sf::Vector2f{ 400.f, 200.f });
//     if (r.ok) Entity enemy = r.entities.front();
//
// Chaque instance reçoit de NOUVEAUX PersistentId ; les liens internes au prefab sont gardés
// (la caméra d'un prefab Player suit le nouveau Player, pas l'ancien).
#include "scene/scene_io.h"

#include <SFML/System/Vector2.hpp>

#include <optional>
#include <string>
#include <vector>

inline constexpr int kPrefabFormatVersion = 1;
inline constexpr const char* kPrefabExtension = ".prefab";

// L'entité + les caméras qui la suivent (Camera2d::targetLock), récursivement : ce qu'on met dans un prefab
std::vector<Entity> prefabGroup(Registry& registry, Entity root);

// La première entité de la liste est la racine (celle placée à la position voulue à l'instanciation)
Json prefabToJson(Registry& registry, const std::vector<Entity>& entities, const std::string& name);
bool savePrefab(Registry& registry, const std::vector<Entity>& entities, const std::string& name,
                const std::string& path, std::string* error = nullptr);

// Crée une copie du prefab. `position` : où mettre la racine (les autres entités gardent leur décalage).
// Sans position : là où elles étaient quand le prefab a été créé.
SceneLoadResult instantiatePrefab(Registry& registry, const Json& prefab, std::optional<sf::Vector2f> position = std::nullopt);
SceneLoadResult instantiatePrefabFile(Registry& registry, const std::string& path, std::optional<sf::Vector2f> position = std::nullopt);

// Nom du prefab (champ "name"), "" si illisible
std::string prefabName(const Json& prefab);
