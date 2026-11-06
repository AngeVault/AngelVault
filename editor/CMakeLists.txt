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
    src/input_settings_dialog.h
    src/tileset_dialog.h
    src/tileset_dialog.cpp
    src/tileset_view.h
    src/tileset_view.cpp
    src/tileset_import.h
    src/tileset_import.cpp
    src/brush_footprint.h
    src/systems_dialog.h
    src/systems_dialog.cpp
    src/input_settings_dialog.cpp
    src/qt_sfml_input.h
    src/qt_sfml_input.cpp
    src/theme_manager.h
    src/theme_manager.cpp
    src/editor_context.h
    src/editor_context.cpp
    src/component_descriptors.h
    src/external_tools.h
    src/external_tools.cpp
    src/component_descriptors.cpp
    src/panels/hierarchy_panel.h
    src/panels/hierarchy_panel.cpp
    src/panels/inspector_panel.h
    src/panels/inspector_panel.cpp
    src/panels/project_panel.h
    src/panels/project_panel.cpp
    src/panels/console_panel.h
    src/panels/console_panel.cpp
    src/scene_toolbar.h
    src/scene_toolbar.cpp
    src/panels/tile_palette_panel.h
    src/panels/tile_palette_panel.cpp
    assets/resources.qrc
        # icônes + QSS (accessibles via ":/resources/...")
)

# Icônes des fichiers du panneau Project : toutes les images de assets/icons/files/
# sont ajoutées automatiquement (":/resources/icons/files/<nom>.png"), sans toucher resources.qrc.
file(GLOB AV_FILE_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/files/*.png")
set(_av_qrc "<RCC>\n  <qresource prefix=\"/resources/icons/files\">\n")
foreach(_icon IN LISTS AV_FILE_ICONS)
  get_filename_component(_name "${_icon}" NAME)
  string(APPEND _av_qrc "    <file alias=\"${_name}\">${_icon}</file>\n")
endforeach()
string(APPEND _av_qrc "  </qresource>\n</RCC>\n")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/file_icons.qrc" CONTENT "${_av_qrc}")
target_sources(AngeVaultEditor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/file_icons.qrc")

set_target_properties(AngeVaultEditor PROPERTIES AUTORCC ON)

# Panneaux façon Visual Studio / Unity : Qt Advanced Docking System (LGPL 2.1, compilée en statique)
# repères de dépôt, onglets glissables, panneaux flottants et masqués sur le côté, disposition mémorisée.
include(FetchContent)
# ADS cherche Qt sous le nom « QT » (Qt5 ou Qt6) : on lui donne le kit déjà trouvé pour l'éditeur
set(QT_VERSION_MAJOR 6)
set(QT_DIR "${Qt6_DIR}")
get_filename_component(_av_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
list(APPEND CMAKE_PREFIX_PATH "${_av_qt_prefix}")
# ADS se lie à Qt6::GuiPrivate (en-têtes privés utiles seulement sous Linux). Depuis Qt 6.9 ce module
# se charge à part ; s'il manque au kit, une cible vide suffit (Windows n'en a pas besoin).
set(QT_NO_PRIVATE_MODULE_WARNING ON)   # avertissement « module privé » : ADS n'en utilise rien sous Windows
find_package(Qt6 COMPONENTS GuiPrivate QUIET)
if(NOT TARGET Qt6::GuiPrivate)
  add_library(Qt6::GuiPrivate INTERFACE IMPORTED)
endif()
set(BUILD_STATIC ON CACHE BOOL "ADS : bibliothèque statique" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "ADS : pas d'exemples" FORCE)
set(ADS_VERSION 4.4.1 CACHE STRING "ADS : version (pas de dépôt git dans l'archive)" FORCE)
FetchContent_Declare(qtads
    URL https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System/archive/refs/tags/4.4.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(qtads)

target_link_libraries(AngeVaultEditor PRIVATE Qt6::Widgets AngeVault::Engine)
target_link_libraries(AngeVaultEditor PRIVATE ads::qtadvanceddocking-qt6)
target_include_directories(AngeVaultEditor PRIVATE src)

# Dossier montré par le panneau Project : le projet source (assets + scripts), pas la copie dans bin/
target_compile_definitions(AngeVaultEditor PRIVATE AV_PROJECT_DIR="${CMAKE_SOURCE_DIR}/projects/rpg")

# Projet ouvert dans l'éditeur : le RPG (projects/rpg/rpg_scene.h), joué avec Exécuter > Jouer
target_link_libraries(AngeVaultEditor PRIVATE rpg_project)   # rpg_scene.h + le code de jeu (rpg_game_code.h)

# Textures -> bin/assets, à côté de l'exécutable : les tilesets (rangés dans projects/sandbox/assets)
# puis les assets du RPG
av_copy_assets(AngeVaultEditor ${CMAKE_SOURCE_DIR}/projects/sandbox/assets)
if(EXISTS ${CMAKE_SOURCE_DIR}/projects/rpg/assets)
  av_copy_assets(AngeVaultEditor ${CMAKE_SOURCE_DIR}/projects/rpg/assets)
endif()

# Exécuter > Lancer rpg.exe : on le compile avec l'éditeur (même dossier bin/, mêmes assets)
if(TARGET rpg)
  add_dependencies(AngeVaultEditor rpg)
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
