#include "game_loop.h"
#include "sandbox_scene.h"

int main() {
    Game game;
    loadSandboxScene(game);
    game.run();
    return 0;
}
