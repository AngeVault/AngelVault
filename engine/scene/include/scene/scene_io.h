#pragma once
// Sauvegarde / chargement d'une scène (.scene, JSON) : entités + composants.
//
//     SceneMeta meta;
//     loadScene(game.registry(), meta, "scenes/main.scene");      // le jeu
//     saveScene(ctx.scene(), meta, "scenes/main.scene");          // l'éditeur (Ctrl+S)
//
// Les liens entre entités passent par PersistentId (voir components/persistent_id.h) :
// le chargement se fait en deux passes (1. créer les entités, 2. lire les composants).
#include "scene/component_serializer.h"

#include <string>
#include <vector>

inline constexpr int kSceneFormatVersion = 1;

// Ce qui, dans le fichier, n'est pas une entité
struct SceneMeta {
    std::string name;
    std::string mapPath;            // map associée (relative au projet), "" = aucune
    Json editor = Json::object();   // réservé à l'éditeur (catégories...), ignoré par le jeu
};

struct SceneLoadResult {
    bool ok = false;
    std::string error;                    // si !ok
    std::vector<std::string> warnings;    // composant inconnu, cible introuvable...
    std::vector<Entity> entities;         // entités créées, dans l'ordre du fichier
};

// Donne un PersistentId à chaque entité vivante qui n'en a pas
void ensurePersistentIds(Registry& registry);

// Écriture (toutes les entités vivantes). Les entités sans PersistentId en reçoivent un.
Json sceneToJson(Registry& registry, const SceneMeta& meta);
std::string sceneToString(Registry& registry, const SceneMeta& meta);
bool saveScene(Registry& registry, const SceneMeta& meta, const std::string& path, std::string* error = nullptr);

// Lecture : AJOUTE les entités du fichier au registry (videz-le avant si besoin, voir clearRegistry).
// En cas d'erreur de lecture, rien n'est ajouté.
SceneLoadResult sceneFromJson(Registry& registry, SceneMeta& meta, const Json& j);
SceneLoadResult sceneFromString(Registry& registry, SceneMeta& meta, const std::string& text);
SceneLoadResult loadScene(Registry& registry, SceneMeta& meta, const std::string& path);

// Briques communes aux scènes et aux prefabs : une liste d'entités <-> tableau JSON "entities".
// Écriture : un lien vers une entité absente de la liste devient null.
// Lecture : freshIds = true donne de NOUVEAUX PersistentId (copie : prefab, copier-coller),
//           les liens internes à la liste sont conservés.
Json entitiesToJson(Registry& registry, const std::vector<Entity>& entities);
SceneLoadResult entitiesFromJson(Registry& registry, const Json& entities, bool freshIds);

// Détruit toutes les entités (tout de suite)
void clearRegistry(Registry& registry);
