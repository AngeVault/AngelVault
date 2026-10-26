add_executable(AngeVaultEditor WIN32
    src/main.cpp
    src/sfml_widget.h
    src/sfml_widget.cpp
    src/customWindow.h
    src/customWindow.cpp
    src/titleBar.h
    src/titleBar.cpp
    src/tilemap_view.h
    src/tilemap_view.cpp
    src/game_view.h
    src/game_view.cpp
    src/theme_manager.h
    src/theme_manager.cpp
    assets/resources.qrc          # icônes + QSS (accessibles via ":/resources/...")
)

set_target_properties(AngeVaultEditor PROPERTIES AUTORCC ON)

target_link_libraries(AngeVaultEditor PRIVATE Qt6::Widgets AngeVault::Engine)

# Exécuter > Jouer fait tourner la scène du Sandbox dans l'éditeur (projects/sandbox/sandbox_scene.h)
target_include_directories(AngeVaultEditor PRIVATE ${CMAKE_SOURCE_DIR}/projects/sandbox)

# Textures (tilesets...) : projects/sandbox/assets -> bin/assets, à côté de l'exécutable
av_copy_assets(AngeVaultEditor ${CMAKE_SOURCE_DIR}/projects/sandbox/assets)

# Exécuter > Lancer Sandbox.exe : on le compile avec l'éditeur (même dossier bin/, mêmes assets)
if(TARGET Sandbox)
  add_dependencies(AngeVaultEditor Sandbox)
endif()

if(WIN32)
  # dwmapi : ombre / coins arrondis / Snap ; shell32 : barre des tâches auto-masquée
  target_link_libraries(AngeVaultEditor PRIVATE dwmapi shell32 user32)

  find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(WINDEPLOYQT)
    add_custom_command(TARGET AngeVaultEditor POST_BUILD
      COMMAND ${WINDEPLOYQT} --no-compiler-runtime $<TARGET_FILE:AngeVaultEditor>)
  endif()
endif()
