# Code de jeu du projet, inclus AUTOMATIQUEMENT (un en-tête est généré : rpg_game_code.h) :
#   behaviors/*.h   comportements (REGISTER_BEHAVIOR)
#   components/*.h  composants de jeu (REGISTER_COMPONENT)
#   systems/*.h     systèmes de jeu (REGISTER_SYSTEM)
# Pour en ajouter : créez le fichier (ou clic droit dans le panneau Project > Créer...), puis recompilez.
file(GLOB AV_RPG_GAME_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/components/*.h ${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/behaviors/*.h ${CMAKE_CURRENT_SOURCE_DIR}/behaviors/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h ${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp)
set(_av_game_header "// Généré par CMake depuis projects/rpg/ (components/, behaviors/, systems/) : ne pas modifier.\n#pragma once\n")
foreach(_f ${AV_RPG_GAME_FILES})
  string(APPEND _av_game_header "#include \"${_f}\"\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/rpg_game_code.h CONTENT "${_av_game_header}")

# Cible « en-têtes du projet » : rpg_scene.h + rpg_game_code.h (utilisée par rpg ET l'éditeur)
add_library(rpg_project INTERFACE)
target_include_directories(rpg_project INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(rpg_project INTERFACE AngeVault::Engine)

if(AV_BUILD_RPG)
  add_executable(rpg main.cpp)
  target_link_libraries(rpg PRIVATE rpg_project)
  av_copy_assets(rpg ${CMAKE_CURRENT_SOURCE_DIR}/assets)
  # Le projet (rpg.angproj) et ses scènes, pour lancer rpg.exe tout seul
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rpg.angproj)
    add_custom_command(TARGET rpg POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/rpg.angproj $<TARGET_FILE_DIR:rpg>/rpg.angproj)
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/scenes)
    add_custom_command(TARGET rpg POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/scenes $<TARGET_FILE_DIR:rpg>/scenes)
  endif()
endif()
