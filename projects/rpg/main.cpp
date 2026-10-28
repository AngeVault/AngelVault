#include "game_loop.h"
#include "rpg_scene.h"

int main()
{
    Game game;
    loadRpgScene(game);   // systèmes + entités : voir rpg_scene.h (partagé avec l'éditeur)
    game.run();
    return 0;
}
