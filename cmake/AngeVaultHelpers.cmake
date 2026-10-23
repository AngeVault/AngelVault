# av_module(Nom DEPS <cibles...>)  ->  cible av_<Nom>, alias AngeVault::<Nom>
#   include/  : en-têtes publics            src/ : *.cpp (facultatif)
#   Aucun .cpp  => module 100 % header (INTERFACE), comme ton Engine actuel.
function(av_module name)
  cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
  if(srcs)
    add_library(av_${name} STATIC ${srcs})
    target_include_directories(av_${name} PUBLIC include)
    target_link_libraries(av_${name} PUBLIC ${ARG_DEPS})
    target_compile_features(av_${name} PUBLIC cxx_std_20)
  else()
    add_library(av_${name} INTERFACE)
    target_include_directories(av_${name} INTERFACE include)
    target_link_libraries(av_${name} INTERFACE ${ARG_DEPS})
    target_compile_features(av_${name} INTERFACE cxx_std_20)
  endif()
  add_library(AngeVault::${name} ALIAS av_${name})
endfunction()

# Copie le dossier assets/ à côté de l'exécutable après chaque build
function(av_copy_assets target dir)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${dir} $<TARGET_FILE_DIR:${target}>/assets)
endfunction()
