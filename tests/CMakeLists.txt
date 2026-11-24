add_executable(test_component_pool test_component_pool.cpp)
target_link_libraries(test_component_pool PRIVATE AngeVault::Core)
add_test(NAME test_component_pool COMMAND test_component_pool)

add_executable(test_math test_math.cpp)
target_link_libraries(test_math PRIVATE AngeVault::Core)
add_test(NAME test_math COMMAND test_math)

add_executable(test_scene test_scene.cpp)
target_link_libraries(test_scene PRIVATE AngeVault::Scene)
add_test(NAME test_scene COMMAND test_scene)

add_executable(test_tilemap test_tilemap.cpp)
target_link_libraries(test_tilemap PRIVATE AngeVault::Tilemap)
add_test(NAME test_tilemap COMMAND test_tilemap)

add_executable(test_camera test_camera.cpp)
target_link_libraries(test_camera PRIVATE AngeVault::Render2D)
add_test(NAME test_camera COMMAND test_camera)

add_executable(test_tile_collision test_tile_collision.cpp)
target_link_libraries(test_tile_collision PRIVATE AngeVault::Tilemap)
add_test(NAME test_tile_collision COMMAND test_tile_collision)

add_executable(test_scene_transition test_scene_transition.cpp)
target_link_libraries(test_scene_transition PRIVATE AngeVault::Scene)
add_test(NAME test_scene_transition COMMAND test_scene_transition)

add_executable(test_interaction test_interaction.cpp)
target_link_libraries(test_interaction PRIVATE AngeVault::Gameplay)
add_test(NAME test_interaction COMMAND test_interaction)

add_executable(test_sprite_animation test_sprite_animation.cpp)
target_link_libraries(test_sprite_animation PRIVATE AngeVault::Render2D)
add_test(NAME test_sprite_animation COMMAND test_sprite_animation)

add_executable(test_inventory test_inventory.cpp)
target_link_libraries(test_inventory PRIVATE AngeVault::Scene)
add_test(NAME test_inventory COMMAND test_inventory)

add_executable(test_settings test_settings.cpp)
target_link_libraries(test_settings PRIVATE AngeVault::Runtime)
add_test(NAME test_settings COMMAND test_settings)

add_executable(test_pickup test_pickup.cpp)
target_link_libraries(test_pickup PRIVATE AngeVault::Gameplay)
add_test(NAME test_pickup COMMAND test_pickup)

add_executable(test_chest test_chest.cpp)
target_link_libraries(test_chest PRIVATE AngeVault::Gameplay)
add_test(NAME test_chest COMMAND test_chest)
