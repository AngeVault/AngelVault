# Bêta — 25/11/2026

objectif de la consigne : un éditeur utilisable et une première version jouable de l'aventure, socle indispensable largement opérationnel

ce qu'il faut présenter (consigne §5, milestone 2) :

1. une démo en direct : lancer l'éditeur, modifier une map, sauvegarder, lancer le mode test, jouer une séquence avec au moins une interaction significative, revenir à l'éditeur
2. les fonctionnalités indispensables terminées
3. les fonctionnalités indispensables encore manquantes
4. les fonctionnalités avancées déjà opérationnelles (et le plan des autres)
5. un planning réaliste des trois dernières semaines

légende : ✅ fait · 🟡 partiel / à vérifier · ❌ manquant

---

## 1. déroulé de la démo (≈ 10 min)

préparer la machine avant : build `dev` à jour, éditeur ouvert sur `projects/rpg/rpg.angproj`, tests passés (`ctest --test-dir out/build/dev`)

### partie éditeur

| étape | ce qu'on montre | où |
|---|---|---|
| 1 | ouvrir le projet rpg, scène `main.scene` | panneau project |
| 2 | naviguer : zoom molette centré souris, déplacement clic droit / molette, grille on/off | vue scène |
| 3 | palette : choisir une tile, placer, remplacer, pipette, gomme, remplissage | palette + vue scène |
| 4 | layers : changer de layer actif (fond / principal / supérieur), montrer qu'il est identifiable, mode ysort du principal | tilemap |
| 5 | autotiling : peindre un terrain autotile, les transitions se posent seules | palette |
| 6 | collisions : tiles solides + colliders d'entités, debug des collisions | inspecteur |
| 7 | placer un objet depuis un prefab (coffre, pnj), le configurer dans l'inspecteur (contenu du coffre, quête, dialogue, transition) | hierarchy + inspecteur |
| 8 | undo / redo (ctrl+z / ctrl+y), docks déplaçables et détachables | — |
| 9 | sauvegarder, fermer la scène, la recharger : tout est restauré | — |
| 10 | lancer le mode test dans le dock game, jouer, stopper, retour à l'éditeur intact | toolbar scène |

### partie jeu (séquence jouable actuelle)

1. le joueur apparaît sur la map extérieure (`main.scene`, map iso + cycle jour-nuit + post-process)
2. il parle au pnj (`e`) : dialogue `dialog.dialog`, la quête `epee_perdue` est acceptée
3. il ramasse la clé du donjon (pickup, l'inventaire s'affiche avec `tab`)
4. il entre dans le donjon par la porte (interaction + transition de map, le joueur est conservé)
5. il ouvre le coffre verrouillé avec la clé : animation d'ouverture, épée récupérée, quête accomplie, récompense (potion)
6. il ressort par la sortie et réapparaît sur le spawn `Sortie donjon`

points à insister : aucune instance n'est codée à la main, tout vient des fichiers `.scene` / `.map` / `.dialog` édités dans l'éditeur ; le jeu standalone et le mode test lisent les mêmes données

---

## 2. checklist des attendus bêta

| attendu bêta (consigne) | statut | détail |
|---|---|---|
| création, modification, sauvegarde, chargement des maps | ✅ | `.map` + `.scene` json, dimensions réglables (`resize`) |
| gestion fonctionnelle des 3 layers | ✅ | layers illimités, nommés fond / principal / supérieur sur la map principale, modes below / ysort, hauteur par layer |
| gestion des collisions | ✅ | tiles solides (`TileCollisionSystem`, y compris layers surélevés) + `Collider2D` / `RigidBody2D` |
| palette de tiles utilisable | ✅ | palette, import de tileset, tiles animées, autotile |
| placement et configuration des objets interactifs | ✅ | coffres, pickups, interactables, transitions, prefabs |
| gestion des pnj et des ennemis | 🟡 | pnj ok (un seul dans la démo) ; **aucun ennemi** |
| chargement des données dans le moteur | ✅ | `scene_io`, `prefab`, `project` (`.angproj`) |
| mode test intégré à l'éditeur | ✅ | dock game, snapshot de la scène, pause, retour à l'éditeur |
| déplacement du personnage et transitions entre maps | ✅ | joueur animé 8 directions, `MapTransition` + `SpawnPoint`, joueur gardé (`DontDestroy`) |
| dialogues avec les pnj | ✅ | composant `Dialog` + module ui (bulles, boîte hud) |
| une quête avec objectif et récompense | ✅ | `QuestGiver` → `QuestObjective`, récompense dans l'inventaire |
| au moins un ennemi combattable | ❌ | ni composant de vie, ni attaque, ni ennemi |
| première version jouable du scénario | 🟡 | séquence pnj → clé → donjon → coffre jouable, mais histoire encore mince |
| architecture stable pour la suite | ✅ | ecs + un module par sous-système, tests unitaires |

---

## 3. socle indispensable

### terminé

- éditeur : création de map, grille, palette, placer / remplacer / sélectionner / supprimer, remplissage, pipette, zoom + déplacement, layers avec layer actif visible, collisions, sauvegarde / chargement
- données : sauvegarde et restauration des maps et des entités, coffres configurables (contenu, clé, durée d'animation), ramassage d'objets, inventaire par cases, interactions à la touche, transitions entre maps
- jeu : mode test depuis l'éditeur et retour, mêmes données éditeur / jeu, déplacement avec collisions, dialogues, une quête acceptée et accomplie, récompense
- démo : une map extérieure (`main`) et un donjon (`donjon`) reliés

### encore manquant

- **ennemi combattable** : composant vie, attaque du joueur, mort / loot de l'ennemi, ennemi configurable dans l'éditeur (le plus urgent)
- **plusieurs pnj** : un seul pnj aujourd'hui, la consigne en demande plusieurs
- **début d'histoire compréhensible** : intro, dialogues plus fournis, conclusion intermédiaire claire
- **affichage en jeu** : quête et inventaire affichés en jeu (aujourd'hui en partie dans la console)
- **scène `montagne`** : pas reliée aux autres maps, et le dialogue parle de la montagne alors que l'épée est dans le donjon → aligner histoire et maps
- nettoyage du projet : `iso_demo.scene`, `nouvelle.scene` et les maps `tilemap_iso*.map` inutilisées
- documentation technique (architecture, format de sauvegarde, features) : à écrire

---

## 4. fonctionnalités avancées

| feature | statut | ce qui existe / ce qui manque |
|---|---|---|
| autotiling | ✅ | modes `Edges16` / `Corners16`, terrain peint, transitions automatiques |
| tiles animées | ✅ | animation par tile dans le tileset |
| objets à plusieurs états visuels | ✅ | coffre fermé → animation → ouvert |
| animations de sprites | ✅ | `SpriteAnimator` par planche, 8 directions, idle / run |
| effets visuels | ✅ | post-process par shader configurable, particules, camera shake |
| élément animé ayant un effet gameplay | ❌ | ex : piège / flamme qui blesse (dépend du système de vie) |
| cycle jour-nuit par shader | 🟡 | `DayNightCycle` + `daynight.frag`, réglable dans l'inspecteur ; manque pnj / ennemis / événements selon l'heure |
| transparence du décor | ❌ | rien encore ; à faire en s'appuyant sur le tri ysort |
| steering | ❌ | module `engine/ai` vide ; wander, seek, flee, arrive, pursuit, obstacle avoidance à faire, réglables dans l'inspecteur et compatibles collisions |
| ergonomie | ✅ | undo / redo, docks escamotables et détachables, raccourcis, inspecteur contextuel, thème |

extensions déjà présentes (bonus, ne compensent pas le socle) : inventaire par cases, système de particules, post-process, réglages d'input et de jeu, prefabs

---

## 5. planning des 3 dernières semaines

| semaine | dates | priorité | contenu |
|---|---|---|---|
| 6 | 26/11 → 02/12 | finir le socle | ennemi combattable (vie, attaque, mort, loot), plusieurs pnj, quête et inventaire affichés en jeu |
| 7 | 03/12 → 09/12 | avancées + contenu | steering (wander pour les pnj, seek / pursuit pour l'ennemi, obstacle avoidance), transparence du décor, pnj / ennemis selon l'heure, scénario complet sur les maps, élément animé qui agit sur le gameplay |
| 8 | 10/12 → 16/12 | stabilisation (gold) | aucune grosse feature : bugs, tests, ergonomie, doc technique, bilan, build sur la machine de démo, répétition de la soutenance |

si la semaine 6 déborde : on réduit les avancées à steering + jour-nuit selon l'heure, la transparence passe en dernier

---

## 6. architecture à expliquer

- `engine/` : un module par sous-système (core ecs, render2d, tilemap, scene, physics2d, input, gameplay, particles, ui, runtime, ai)
- `editor/` : éditeur Qt qui utilise le moteur, le moteur ne dépend jamais de l'éditeur
- `projects/rpg/` : le jeu = données (`.angproj`, `.scene`, `.map`, `.prefab`, `.dialog`) + ce qui est propre au jeu (behaviors, composants spécifiques)
- composants réfléchis (`reflect.h`) : l'inspecteur et la sérialisation se génèrent depuis la déclaration du composant, ajouter un champ le rend éditable et sauvegardé sans autre code
- format de sauvegarde : json versionné (`format` + `version`), une scène référence sa map et liste ses entités avec leurs composants
- tests : `tests/` (tilemap, collisions de tiles, scène, transitions, interactions, coffre, pickup, inventaire, quête, animation, caméra, réglages)

## 7. qui présente quoi

chaque membre doit pouvoir expliquer ses systèmes (d'après l'historique git, à ajuster entre nous) :

- **Ange** : tilemap (layers, hauteur, tiles animées, collisions), gameplay (interactions, coffres, pickups, inventaire, quêtes), animation de sprites, ergonomie de l'éditeur
- **Tom** : rendu (render texture, post-process, caméra ecs), changement de map, collisions des tiles solides, docks de l'éditeur et hierarchy, maps de la démo
- **CitrouilleBleu** : module ui (texte, boutons, dialogues), transitions, parties de l'éditeur

## 8. à vérifier juste avant

- [ ] build propre depuis zéro avec le preset `dev`
- [ ] tous les tests passent
- [ ] la séquence de jeu se joue d'une traite sans bug bloquant
- [ ] une sauvegarde puis un rechargement en direct ne perdent rien
- [ ] retour à l'éditeur après le mode test sans perte de la scène
- [ ] la scène de démarrage du projet est bien `main.scene`
