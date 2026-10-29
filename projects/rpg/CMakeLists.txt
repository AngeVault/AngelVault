# Comportements du projet : TOUS les .h de behaviors/ sont inclus automatiquement
# (un en-tête est généré : rpg_behaviors.h). Pour en ajouter un : créez le fichier
# (ou clic droit dans le panneau Project > Créer un behavior), puis recompilez.
file(GLOB AV_RPG_BEHAVIOR_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/behaviors/*.h ${CMAKE_CURRENT_SOURCE_DIR}/behaviors/*.hpp)
list(SORT AV_RPG_BEHAVIOR_FILES)
set(_av_behaviors_header "// Généré par CMake depuis projects/rpg/behaviors/ : ne pas modifier à la main.\n#pragma once\n")
foreach(_f ${AV_RPG_BEHAVIOR_FILES})
  string(APPEND _av_behaviors_header "#include \"${_f}\"\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/rpg_behaviors.h CONTENT "${_av_behaviors_header}")

# Cible « en-têtes du projet » : rpg_scene.h, behaviors/, rpg_behaviors.h (utilisée par rpg ET l'éditeur)
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
