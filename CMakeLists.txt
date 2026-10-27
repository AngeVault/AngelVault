cmake_minimum_required(VERSION 3.28)
project(AngeVault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

option(AV_BUILD_EDITOR  "Build the Qt editor (needs Qt 6)" ON)
option(AV_BUILD_SANDBOX "Build the sandbox project"        ON)
option(AV_BUILD_TESTS   "Build the tests"                  ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Dependencies)
include(AngeVaultHelpers)

add_subdirectory(engine)               # LE MOTEUR : un module par sous-système
add_subdirectory(projects/sandbox)     # UN PROJET : main.cpp + behaviors (exécutable Sandbox)
add_subdirectory(projects/rpg)

if(AV_BUILD_EDITOR)
  # Windows : si Qt n'est pas indiqué, on cherche le dernier kit MSVC dans C:/Qt/6.x.y/msvc*_64
  if(WIN32 AND NOT Qt6_DIR AND NOT CMAKE_PREFIX_PATH)
    file(GLOB _qt_kits "C:/Qt/6.*/msvc*_64")
    list(SORT _qt_kits COMPARE NATURAL ORDER DESCENDING)
    if(_qt_kits)
      list(GET _qt_kits 0 _qt_kit)
      list(APPEND CMAKE_PREFIX_PATH "${_qt_kit}")
      message(STATUS "Qt detecte automatiquement : ${_qt_kit}")
    endif()
  endif()

  find_package(Qt6 COMPONENTS Widgets QUIET)
  message(STATUS "DIAG: Qt6_FOUND=${Qt6_FOUND} Qt6_DIR=${Qt6_DIR} compilateur=${CMAKE_CXX_COMPILER_ID} MSVC=${MSVC}")

  # Un kit Qt « msvc » ne se lie qu'avec le compilateur MSVC (pas avec MinGW, le g++ fourni par CLion).
  set(_av_qt_ok ${Qt6_FOUND})
  if(Qt6_FOUND AND WIN32 AND NOT MSVC AND "${Qt6_DIR}" MATCHES "msvc")
    message(WARNING
            "Kit Qt MSVC (${Qt6_DIR}) mais compilateur ${CMAKE_CXX_COMPILER_ID} : l'editeur est ignore.\n"
            "  CLion : Settings > Toolchains > + > Visual Studio (amd64), puis profil CMake > Toolchain = Visual Studio,\n"
            "  puis Reset Cache and Reload Project.\n"
            "  Visual Studio : ouvrir le dossier et choisir le preset x64-debug.")
    set(_av_qt_ok FALSE)
  endif()

  if(_av_qt_ok)
    set(CMAKE_AUTOMOC ON)
    add_subdirectory(editor)
  elseif(NOT Qt6_FOUND)
    message(WARNING
            "Qt 6 introuvable : l'editeur n'est PAS construit (le moteur et le Sandbox le sont).\n"
            "  -> installe Qt 6 (kit MSVC 2022 64-bit) puis indique son dossier, par exemple :\n"
            "     cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64 ...\n"
            "     ou copie CMakeUserPresets.example.json en CMakeUserPresets.json et corrige le chemin.")
  endif()
endif()

if(AV_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(TARGET AngeVaultEditor)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT AngeVaultEditor)
endif()