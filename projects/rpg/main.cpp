#include "game_loop.h"
#include "rpg_scene.h"
#include "scene/project.h"
#include "scene/scene_io.h"

#include <iostream>

// rpg.exe [chemin/vers/rpg.angproj] [chemin/vers/scene.scene]
//   Sans argument : rpg.angproj à côté de l'exe (copié par CMake avec scenes/), et sa scène de départ.
//   L'éditeur (Ctrl+F5) passe le .angproj du projet source et la scène ouverte (enregistrée juste avant).
int main(int argc, char** argv)
{
    const std::string projectPath = argc > 1 ? argv[1] : "rpg.angproj";
    Project project;
    std::string error;
    const bool hasProject = project.load(projectPath, &error);
    if (!hasProject) std::cerr << "[RPG] " << error << " : scene de rpg_scene.h\n";

    const ProjectWindow& w = project.window;
    Game game(sf::Vector2u{ w.width, w.height }, hasProject ? w.title : "RPG", w.vsync);
    registerRpgSystems(game);

    if (hasProject) {
        loadRpgInput(game.input(), project.resolve(project.inputPath));
        SceneMeta meta;
        const std::string scenePath = argc > 2 ? std::string(argv[2]) : project.resolve(project.startScene);
        const SceneLoadResult scene = loadScene(game.registry(), meta, scenePath);
        for (const std::string& warning : scene.warnings) std::cerr << "[RPG] " << warning << "\n";
        if (!scene.ok) {
            std::cerr << "[RPG] " << scene.error << " : scene de rpg_scene.h\n";
            loadRpgEntities(game.registry());
        }
    } else {
        loadRpgInput(game.input());
        loadRpgEntities(game.registry());
    }

    game.run();
    return 0;
}
