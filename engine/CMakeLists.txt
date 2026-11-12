# Un module = un dossier = une cible AngeVault::<Nom>. Les dépendances sont dans chaque CMakeLists.
add_subdirectory(core)        # ECS, composants de base, scripts
add_subdirectory(physics2d)   # collisions, corps rigides
add_subdirectory(particles)   # particules
add_subdirectory(tilemap)     # carte de tiles
add_subdirectory(input)       # entrées : actions nommées (clavier, souris, manette)
add_subdirectory(render2d)    # rendu SFML : seul module qui dessine
add_subdirectory(scene)       # sauvegarde : scènes .scene, projet .angproj (JSON)
add_subdirectory(runtime)     # boucle de jeu (Game)

# Modules prévus, pas encore créés (un README par dossier) :
#   assets  animation  audio  ui  ai

add_library(av_engine INTERFACE)
add_library(AngeVault::Engine ALIAS av_engine)
target_link_libraries(av_engine INTERFACE
    AngeVault::Core AngeVault::Physics2D AngeVault::Particles AngeVault::Tilemap
    AngeVault::Render2D AngeVault::Runtime AngeVault::Input AngeVault::Scene)
