# 📝 Écrire un commit — AngeVault

**Format**
```
<gitmoji> <type>(<scope>): <résumé court>

- <feature 1> : ce que ça fait, comment ça marche
- <feature 2> : ...
```
- résumé en minuscule, sans point, moins de 70 caractères
- une ligne par feature dans la description (pas obligatoire pour un petit fix)
- un commit = un sujet

**Types**
- ✨ `feat` nouvelle fonctionnalité
- 🐛 `fix` correction de bug
- ⚡️ `perf` performance
- ♻️ `refactor` code réorganisé, même comportement
- 🎨 `style` mise en forme, structure du code
- 🔥 `remove` code ou fichiers supprimés
- 💄 `ui` interface de l'éditeur
- 📝 `docs` documentation
- 🔧 `chore` config, CMake, .gitignore
- ✅ `test` tests

**Scopes**
`editor` `engine` `render2d` `tilemap` `scene` `physics2d` `input` `runtime` `rpg`

**Templates**
```
✨ feat(editor): 

- 
```
```
🐛 fix(): 

- cause : 
- correction : 
```
```
⚡️ perf(): 

- avant : 
- maintenant : 
```

**Exemple**
```
⚡️ perf(render2d): tiles dessinées par batch

- Orthogonal : un draw call par tileset
- Isométrique : tiles de même texture regroupées, ordre arrière -> avant gardé
```
