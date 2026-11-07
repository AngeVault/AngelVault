av_module(Render2D DEPS AngeVault::Core AngeVault::Physics2D AngeVault::Tilemap AngeVault::Input SFML::Graphics)

# Shaders du moteur -> bin/shaders/, à côté des exécutables (relancé à chaque build : un .frag modifié est recopié)
add_custom_target(av_render2d_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
add_dependencies(av_Render2D av_render2d_shaders)
