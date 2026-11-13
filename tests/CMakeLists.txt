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
