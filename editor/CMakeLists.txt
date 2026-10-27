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
    src/panels/tile_palette_panel.h
    src/panels/tile_palette_panel.cpp
    assets/resources.qrc          # icônes + QSS (accessibles via ":/resources/...")
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

target_link_libraries(AngeVaultEditor PRIVATE Qt6::Widgets AngeVault::Engine)
target_include_directories(AngeVaultEditor PRIVATE src)

# Dossier montré par le panneau Project : le projet source (assets + scripts), pas la copie dans bin/
target_compile_definitions(AngeVaultEditor PRIVATE AV_PROJECT_DIR="${CMAKE_SOURCE_DIR}/projects/sandbox")

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
