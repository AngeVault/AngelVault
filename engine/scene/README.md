# scene

Sauvegarde / chargement en JSON : scènes (`.scene` : entités + composants) et projet (`.angproj`).
Utilisé par l'éditeur (Ctrl+S, Ouvrir, F5) et par le jeu (`rpg.exe` lit le projet et sa scène).

- `scene/scene_io.h` : `saveScene` / `loadScene`, `sceneToString` / `sceneFromString` (mode Jouer de l'éditeur)
- `scene/component_serializer.h` : registre « nom de composant -> JSON » ; ajoutez-y vos composants
- `scene/project.h` : fichier `.angproj`, chemins relatifs au projet, `Project::create`
- Liens entre entités : `PersistentId` (core/components/persistent_id.h)
- Scripts : `REGISTER_BEHAVIOR(MaClasse)` (core/ecs/behavior_factory.h)

Détails et format des fichiers : `docs/projet_et_sauvegarde.md`. Test : `tests/test_scene.cpp`.

**Dépend de :** Core, Physics2D, Render2D, nlohmann_json
