# Projet `.angproj` et sauvegarde des scènes / maps

Ce document décrit comment un projet AngeVault est organisé sur le disque, et comment les scènes (entités + composants) et les tilemaps sont sauvegardées et rechargées.

> **État (implémenté)** : `.angproj` (§3), scènes `.scene` avec `PersistentId` (§4) et `BehaviorFactory` (§4.4), serializers des composants (§5), menus Fichier, étoile `*` et F5 sur une copie de la scène (§8), `rpg.exe` qui lit le projet. Code : `engine/scene/`, test : `tests/test_scene.cpp`.
> **Pas encore fait** : la sauvegarde de la map (§6, les tiles peintes ne sont pas encore enregistrées), les migrations de version (§7), l'export binaire.

Objectifs :

- l'éditeur ouvre un **projet** (un fichier `.angproj`) au lieu d'avoir `projects/rpg` codé en dur ;
- ce qu'on fait dans l'éditeur (entités, catégories, caméra liée, tiles peintes) est **enregistré** dans des fichiers ;
- le mode Jouer (F5) et `rpg.exe` chargent **ces fichiers**, plus `rpg_scene.h` ;
- le moteur reste en **ECS** : on sauvegarde des données (composants), jamais des objets.

---

## 1. Organisation d'un projet sur le disque

```
projects/rpg/
├── rpg.angproj              <- le fichier projet (ce que l'éditeur ouvre)
├── assets/
│   ├── input.cfg            <- actions d'entrée (existe déjà)
│   ├── textures/  ...
│   └── tilesets/  ...
├── scenes/
│   ├── village.scene        <- entités + composants
│   └── donjon.scene
├── maps/
│   ├── village.map          <- tilemap (grille de tiles)
│   └── donjon.map
├── behaviors/               <- scripts C++ (PlayerBehavior...) : compilés, pas sauvegardés
├── main.cpp
└── CMakeLists.txt
```

Règles :

- **Tous les chemins sont relatifs** au dossier du `.angproj`, par exemple `scenes/village.scene` et jamais `C:\Users\...`. Le projet peut alors être déplacé ou partagé (git), et `rpg.exe` le retrouve à côté de lui.
- **Une scène ≠ une map** :
  - la scène dit *quelles entités existent* ;
  - la map dit *quelles tiles sont peintes* ;
  - une scène référence sa map par son chemin, et plusieurs scènes peuvent réutiliser la même map.
- Le code (behaviors, systèmes) reste en C++ et est compilé. Les fichiers ne contiennent que des **données**, et les scripts y sont désignés **par leur nom** (voir §4.4).

---

## 2. Format : JSON

Je conseille **JSON** pour `.angproj` et `.scene`, avec la bibliothèque header-only [nlohmann/json](https://github.com/nlohmann/json). Elle s'ajoute comme SFML dans `cmake/Dependencies.cmake` :

```cmake
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3 GIT_SHALLOW ON SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SFML glm json)
# puis dans le module qui sérialise : DEPS ... nlohmann_json::nlohmann_json
```

Pourquoi JSON plutôt qu'un format maison comme `input.cfg` :

- les scènes sont **imbriquées** (entité → composants → champs → vecteurs), ce qu'un format ligne par ligne gère mal ;
- c'est lisible et modifiable à la main, et les diffs git restent propres ;
- on peut passer plus tard à un binaire (MessagePack, avec la même lib) pour le jeu final, sans changer le code de sérialisation.

> L'éditeur a aussi `QJsonDocument` (Qt). Mais le **jeu** (`rpg.exe`) n'a pas Qt : la sérialisation doit vivre dans le moteur, donc pas de Qt.

---

## 3. Le fichier projet `.angproj`

Petit fichier : il décrit le projet, il ne contient pas les scènes.

```json
{
  "format": "angeVault.project",
  "version": 1,
  "name": "RPG",
  "engineVersion": "0.1.0",

  "startScene": "scenes/village.scene",
  "scenes": [
    "scenes/village.scene",
    "scenes/donjon.scene"
  ],

  "paths": {
    "assets": "assets",
    "input": "assets/input.cfg"
  },

  "window": {
    "title": "RPG",
    "size": [1920, 1080],
    "vsync": true,
    "fullscreen": false
  },

  "rendering": {
    "tileSize": 16,
    "clearColor": "#000000"
  }
}
```

| Champ | Rôle |
|---|---|
| `format` / `version` | savoir ce qu'on lit, et migrer les vieux fichiers (§7) |
| `startScene` | la scène chargée au lancement de `rpg.exe` et par F5 |
| `scenes` | liste affichée dans l'éditeur (Fichier > Ouvrir une scène) |
| `paths.input` | remplace le `"assets/input.cfg"` codé en dur dans `rpg_scene.h` |
| `window` | remplace le `1920x1080` codé en dur dans `Game::Game()` |

Ce que **l'éditeur** retient de son côté, dans `QSettings` et pas dans le projet : projets récents, disposition des panneaux, thème. Ce sont des préférences de la personne, pas du projet.

---

## 4. Le fichier scène `.scene`

### 4.1 Exemple

```json
{
  "format": "angeVault.scene",
  "version": 1,
  "name": "Village",
  "map": "maps/village.map",

  "entities": [
    {
      "id": 1001,
      "components": {
        "Name":        { "name": "Player" },
        "Transform2D": { "position": [200, 100], "rotation": 0, "scale": [1, 1], "depth": 0 },
        "Geometry":    { "layer": 1, "shape": "Sprite",
                         "texture": "player", "rect": [0, 0, 32, 64], "tint": "#FFFFFFFF" },
        "RigidBody2D": { "mass": 1, "useGravity": false },
        "Collider2D":  { "shape": "Box", "halfSize": [16, 32] },
        "Script":      { "behavior": "PlayerBehavior", "properties": { "speed": 250 } }
      }
    },
    {
      "id": 1002,
      "components": {
        "Name":        { "name": "CameraEntity" },
        "Transform2D": { "position": [200, 100] },
        "Camera2d":    { "zoom": 1, "minZoom": 0.25, "maxZoom": 4, "zoomStep": 0.5,
                         "worldMin": [0, 0], "worldMax": [3000, 3000],
                         "targetLock": 1001 }
      }
    }
  ],

  "editor": {
    "categories": ["Personnages", "Caméras"],
    "entityCategory": { "1001": "Personnages" },
    "camera": { "position": [640, 360], "zoom": 0.5 }
  },

  "systems": ["Physics", "Collision", "Script", "Camera"]
}
```

### 4.2 Les identifiants d'entité (point important)

`Entity { index, generation }` est un identifiant **de session** : il dépend de l'ordre de création et change au rechargement. On ne peut donc pas l'écrire tel quel.

Solution : un **id persistant** par entité, sous forme d'un composant (donc toujours de l'ECS) :

```cpp
// engine/core/include/components/persistent_id.h
struct PersistentId { uint64_t value = 0; };   // unique dans la scène, jamais réutilisé
```

- L'éditeur donne un `PersistentId` à chaque entité créée (compteur `nextId` sauvegardé dans la scène, ou un nombre aléatoire 64 bits).
- **À la sauvegarde**, tout champ qui contient une `Entity` est écrit avec le `PersistentId` de l'entité visée, par exemple `"targetLock": 1001`.
- **Au chargement**, en deux passes :
  1. créer toutes les entités et remplir une table `PersistentId → Entity` ;
  2. charger les composants, en traduisant chaque id référencé grâce à cette table.

C'est ce qui permet à la caméra liée au Player de survivre à un enregistrement / rechargement.

### 4.3 Section `editor`

Les catégories de la Hierarchy, la position de la caméra de l'éditeur, etc. sont des données d'**éditeur**. Le jeu les **ignore**. On les garde dans la scène, à part, pour les retrouver à l'ouverture.

### 4.4 Les scripts (`Script` / `IBehavior`)

Un behavior est une classe C++ : on ne peut pas l'écrire dans un fichier. On écrit **son nom** et ses `PROPERTY`. Il faut donc une **fabrique** qui recrée un behavior à partir de son nom :

```cpp
// engine/core/include/ecs/behavior_factory.h
class BehaviorFactory {
public:
    using Creator = std::function<std::unique_ptr<IBehavior>()>;
    static BehaviorFactory& get();
    void add(const std::string& name, Creator c);
    std::unique_ptr<IBehavior> create(const std::string& name) const;   // nullptr si inconnu
    std::vector<std::string> names() const;                             // pour l'Inspector
};

// Une ligne par behavior, à côté de sa classe :
#define REGISTER_BEHAVIOR(T) \
    static const bool _reg_##T = (BehaviorFactory::get().add(#T, [] { return std::make_unique<T>(); }), true);
```

```cpp
// projects/rpg/behaviors/player_behaviors.h
class PlayerBehavior : public IBehavior { ... };
REGISTER_BEHAVIOR(PlayerBehavior)
```

Les `PROPERTY` existent déjà (`IBehavior::m_properties` avec nom, type et pointeur). Il suffit de les parcourir pour les écrire et les relire. Une propriété absente du fichier garde sa valeur par défaut, et une propriété inconnue est ignorée.

---

## 5. La sérialisation des composants (côté moteur)

### 5.1 Un registre « nom de composant → save / load »

Même idée que les `ComponentDesc` de l'éditeur, mais **dans le moteur**, sans Qt, pour que le jeu puisse charger :

```cpp
// engine/scene/include/scene/component_serializer.h
using json = nlohmann::json;

struct SaveContext { std::function<uint64_t(Entity)> idOf; };      // Entity -> PersistentId
struct LoadContext { std::function<Entity(uint64_t)> entityOf; };  // PersistentId -> Entity

struct ComponentSerializer {
    std::string name;                                                      // "Transform2D"
    std::function<bool(Registry&, Entity)> has;
    std::function<json(Registry&, Entity, const SaveContext&)> save;
    std::function<void(Registry&, Entity, const json&, const LoadContext&)> load;  // ajoute le composant
};

class SerializerRegistry {
public:
    static SerializerRegistry& get();
    template<typename T> void add(std::string name, auto save, auto load);
    const ComponentSerializer* find(const std::string& name) const;
    const std::vector<ComponentSerializer>& all() const;
};
```

### 5.2 Deux façons d'écrire un serializer

**a) Automatique, pour les composants simples réfléchis.** Tu as déjà `reflectComponent<T>()`, qui renvoie des `PropertyInfo` avec nom, type et pointeur. Un seul code générique sait alors lire et écrire `Float / Int / Bool / Vec2f / String` :

```cpp
template<typename T>
void addReflected(const std::string& name) {
    SerializerRegistry::get().add<T>(name,
        [](T& c, const SaveContext&) {
            json j;
            for (PropertyInfo& p : reflectComponent<T>(c).fields) writeProperty(j, p);
            return j;
        },
        [](T& c, const json& j, const LoadContext&) {
            for (PropertyInfo& p : reflectComponent<T>(c).fields)
                if (j.contains(p.name)) readProperty(j[p.name], p);
        });
}
// addReflected<RigidBody2D>("RigidBody2D");
```

**b) À la main, pour les cas spéciaux :**

- `Transform2D` : la position est privée, donc on passe par `getPosition()` / `setPosition()` ;
- `Geometry` et `Collider2D` : ils contiennent un `std::variant`, donc on écrit `"shape": "Box"` puis les champs de la forme, comme la liste « Forme » de l'Inspector ;
- `Camera2d.targetLock` : c'est une `Entity`, donc on passe par `ctx.idOf()` / `ctx.entityOf()` ;
- `Script` : on passe par `BehaviorFactory` (§4.4).

Ajouter un composant au moteur revient alors à écrire un `reflectComponent<T>` (ou un serializer à la main) et à faire **une** ligne d'enregistrement.

### 5.3 Les fonctions de haut niveau

```cpp
// engine/scene/include/scene/scene_io.h
struct SceneData {                 // ce qui n'est pas des entités
    std::string name;
    std::string mapPath;
    nlohmann::json editor;         // gardé tel quel pour l'éditeur, ignoré par le jeu
};

bool saveScene(const Registry& reg, const SceneData& meta, const std::string& path, std::string* error = nullptr);
bool loadScene(Registry& reg, SceneData& meta, const std::string& path, std::string* error = nullptr);

// Variantes en mémoire (utiles pour le mode Jouer, voir §8)
std::string sceneToString(const Registry& reg, const SceneData& meta);
bool sceneFromString(Registry& reg, SceneData& meta, const std::string& text, std::string* error = nullptr);
```

Côté gestion d'erreurs :

- un composant inconnu (renommé, supprimé du code) donne un **avertissement** dans la Console, et la scène se charge quand même ;
- un JSON invalide fait renvoyer `false` avec un message, et la scène actuelle n'est pas touchée.

---

## 6. Le fichier map `.map`

La tilemap peut être grande : la stocker en JSON avec un nombre par case serait lourd. Il y a deux options.

### Option A — JSON + données compressées (conseillée pour commencer)

```json
{
  "format": "angeVault.map",
  "version": 1,
  "width": 200,
  "height": 120,
  "tileSize": 16,
  "tileset": "assets/tilesets/overworld",
  "types": ["None", "Sand", "Water", "Grass", "BrickPath", "DirtPath", "DonjonFloor", "Wall", "Path"],
  "encoding": "rle-base64",
  "tiles": "AwDIAQQB...."
}
```

- `types` : les noms des `TileType` dans l'ordre. Si on ajoute ou réordonne un type dans l'enum, les vieilles maps restent lisibles, car on relit **par nom** et pas par numéro.
- `tiles` : un octet par case (le `TileType`, plus le bit `TF_SOLID` si besoin), compressé en **RLE**, c'est-à-dire en paires (nombre, valeur). Les maps ont de grandes zones identiques (herbe, eau), donc ça compresse très bien. Le tout est ensuite encodé en base64 pour tenir dans le JSON.
- On ne sauvegarde **que la couche logique** (le type de chaque case). Les pièces visuelles du dual-grid (`VertexPiece`, masques 1..15) se **recalculent** au chargement, comme quand on peint.

### Option B — binaire

Un en-tête (`"AVMP"`, version, largeur, hauteur, tileSize), suivi des octets bruts ou RLE. C'est plus compact et plus rapide, mais pas lisible. À garder pour plus tard, par exemple pour l'export du jeu final.

### Plus tard : map infinie en chunks

Pour la « map infinie » (agrandir aussi vers la gauche et le haut), on découpe en **chunks** de 32×32 :

```json
"chunkSize": 32,
"chunks": { "0,0": "<rle>", "-1,0": "<rle>", "3,-2": "<rle>" }
```

Seuls les chunks non vides sont écrits. Les coordonnées négatives deviennent alors possibles **sans décaler** les tiles existantes ni les entités. C'est la limitation actuelle de `TileMap` (`consumeGrowOffset`).

---

## 7. Versions et migrations

Chaque fichier a `"format"` et `"version"`. Au chargement :

```cpp
int v = j.value("version", 1);
if (v > kSceneVersion) -> erreur "fichier créé par une version plus récente"
if (v < 2) migrateV1toV2(j);   // ex. : "Camera2D" renommé "Camera2d"
if (v < 3) migrateV2toV3(j);
```

À la sauvegarde, on écrit toujours la version **actuelle**. Les vieux fichiers sont donc mis à jour à leur prochain enregistrement.

---

## 8. Intégration dans l'éditeur

### Menus (ils existent déjà dans la barre de titre, il reste à les brancher)

| Menu | Action |
|---|---|
| Fichier > Nouveau projet | choisir un dossier et un nom, puis créer `nom.angproj`, `assets/`, `scenes/`, `maps/`, une scène et une map vides |
| Fichier > Ouvrir... | choisir un `.angproj`, puis charger `startScene` (et l'ajouter aux **Récents**) |
| Fichier > Enregistrer (Ctrl+S) | écrire la scène courante et sa map |
| Fichier > Enregistrer sous... | même chose sous un nouveau nom de scène |
| Fichier > Récents | les derniers `.angproj`, gardés dans `QSettings` |

### Modifications non enregistrées

- `EditorContext` garde un indicateur `dirty`, mis à `true` par `sceneChanged`, `entityModified` et une modification de la tilemap, et remis à `false` après un enregistrement.
- Le titre de la fenêtre affiche une étoile quand il y a des modifications non enregistrées, par exemple `AngeVault Editor — village.scene *`.
- En quittant ou en ouvrant un autre projet avec des modifications non enregistrées, on propose « Enregistrer / Ne pas enregistrer / Annuler ».

### Écriture sûre

Il faut toujours écrire dans un fichier temporaire, puis remplacer. Sinon, un crash pendant l'écriture laisse une scène à moitié écrite. Dans l'éditeur, `QSaveFile` le fait déjà (c'est ce qu'utilise l'Input Manager). Dans le moteur, on écrit `village.scene.tmp` puis on fait `std::filesystem::rename`.

### Le mode Jouer (F5) charge enfin la scène de l'éditeur

Aujourd'hui, F5 recharge `rpg_scene.h`. Avec la sérialisation :

```cpp
// au moment de F5
std::string snapshot = sceneToString(ctx.scene(), ctx.sceneData());
gameView = new GameView(snapshot);      // le jeu charge une COPIE de la scène éditée
```

- Le jeu tourne sur sa propre copie : bouger le Player en jeu ne modifie pas la scène de l'éditeur, comme le mode Play d'Unity.
- Pas besoin d'enregistrer avant de jouer.

### Côté jeu (`rpg.exe`)

`main.cpp` n'a plus de scène en dur :

```cpp
int main() {
    Project project;
    if (!project.load("rpg.angproj")) return 1;
    Game game(project.window);
    registerRpgSystems(game);                    // les systèmes restent du C++
    game.input().loadFromFile(project.inputPath);
    loadScene(game.registry(), project.startScene);
    game.run();
}
```

CMake copie le `.angproj`, `scenes/` et `maps/` à côté de l'exe, comme `assets/` aujourd'hui (`av_copy_assets`).

---

## 9. Où mettre le code

| Quoi | Où | Dépend de |
|---|---|---|
| `PersistentId`, `BehaviorFactory` | `engine/core` | — |
| `SerializerRegistry`, `saveScene` / `loadScene`, `Project` | **nouveau module `engine/scene`** (prévu dans `engine/CMakeLists.txt`) | Core, Physics2D, Render2D, nlohmann_json |
| `saveMap` / `loadMap` (RLE, base64) | `engine/tilemap` | nlohmann_json |
| Menus, `dirty`, boîtes de dialogue, F5 avec copie de la scène | `editor/` | Qt + `AngeVault::Scene` |

Le module `scene` ne doit **pas** dépendre de Qt : `rpg.exe` en a besoin.

---

## 10. Ordre conseillé

1. **Map seule** : `saveMap` / `loadMap` (JSON + RLE) et Ctrl+S dans l'éditeur. Résultat immédiat : les tiles peintes restent d'une session à l'autre.
2. **`PersistentId` + serializers** de `Name`, `Transform2D` et `Camera2d`. Ça suffit pour tester la caméra liée (`targetLock`).
3. **Serializers restants** : `Geometry` et `Collider2D` (variants), `RigidBody2D` (automatique par réflexion), `Script` (+ `BehaviorFactory`).
4. **`.angproj`** : Ouvrir / Nouveau projet / Récents, et `rpg.exe` qui lit le projet.
5. **F5 sur une copie** de la scène éditée.
6. Plus tard : chunks pour la map infinie, export binaire, et undo/redo, qui pourra réutiliser `sceneToString` pour les instantanés.

### Tests à prévoir (dans `tests/`)

- **aller-retour** : créer une scène, `sceneToString`, `sceneFromString` dans un nouveau `Registry`, et vérifier que tous les champs sont identiques ;
- **`targetLock`** : il pointe toujours vers la bonne entité après rechargement, même si l'ordre de création a changé ;
- **composant inconnu** dans le fichier : la scène se charge quand même, avec un avertissement ;
- **RLE** : encoder puis décoder une map aléatoire redonne la même map, et une map vide prend peu de place.

---

## 11. Behaviors et prefabs (implémenté)

### Behaviors : un fichier = un (ou plusieurs) comportement(s)

- Tous les `.h` de `projects/rpg/behaviors/` sont inclus **automatiquement** : CMake génère `rpg_behaviors.h`, qui sert au jeu et à l'éditeur. Il n'y a plus de liste à tenir à jour.
- Une entité peut avoir **plusieurs** comportements (`Script::behaviors`), comme plusieurs scripts sur un GameObject Unity :
  ```cpp
  auto& script = registry.addComponent<Script>(e);
  script.add<PlayerBehavior>();
  script.add<EnemyAI>().speed = 80.f;
  if (auto* ai = script.get<EnemyAI>()) { ... }
  ```
- Créer un comportement :
  1. dans le panneau Project, clic droit > **Créer un behavior C++...** : le fichier part d'un modèle, avec `PROPERTY` et `REGISTER_BEHAVIOR` ;
  2. recompiler (c'est du C++) ;
  3. **glisser le fichier `.h`** sur une entité : dans la Hierarchy, dans l'Inspector ou dans la vue de la scène.
- Le lien entre un fichier et sa classe passe par `REGISTER_BEHAVIOR`, qui retient `__FILE__`.
- Dans la scène :
  ```json
  "Script": { "behaviors": [ { "name": "PlayerBehavior", "properties": { "speed": 250 } } ] }
  ```
  L'ancien format, à un seul `"behavior"`, se lit toujours.

### Prefabs (`.prefab`)

- **Créer** :
  - glisser une entité de la Hierarchy vers le panneau Project (le prefab est créé dans le dossier ouvert) ;
  - ou clic droit sur l'entité > **Créer un prefab**, qui le range dans `prefabs/`.
  - Les caméras qui suivent l'entité sont incluses dans le prefab.
- **Utiliser dans l'éditeur** :
  - glisser le `.prefab` dans la vue de la scène, où il apparaît sous la souris (Ctrl : aligné sur la demi-case) ;
  - ou le glisser dans la Hierarchy.
- **Utiliser en jeu**, depuis un behavior :
  ```cpp
  #include "scene/prefab.h"
  auto r = instantiatePrefabFile(registry, "prefabs/Enemy.prefab", sf::Vector2f{ x, y });
  ```
- Chaque instance reçoit de nouveaux `PersistentId`. Les liens internes sont recréés : la caméra de l'instance suit le Player de l'instance.
- **Pas encore fait** : modifier un prefab ne met pas à jour les instances déjà posées. Ce sont des copies, pas des liens comme dans Unity.
