# scene (module prévu, pas encore créé)

Sérialisation JSON des scènes (entités, composants, tilemap), identifiants stables, prefabs, registre de comportements (nom -> fabrique) pour sauvegarder Script. Même fichier pour l'éditeur et le jeu.

**Dépend de :** Core, Tilemap, nlohmann_json
**Pour le créer :** `av_module(Scene DEPS ...)` dans un `CMakeLists.txt` de ce dossier, puis `add_subdirectory(scene)` dans `engine/CMakeLists.txt`.
