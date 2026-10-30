include(FetchContent)

# Libs statiques : rien à livrer à part Qt (voir windeployqt dans editor/CMakeLists.txt)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO   OFF CACHE BOOL "" FORCE)   # passer à ON quand le module audio existera
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0 GIT_SHALLOW ON SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1 GIT_SHALLOW ON SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP ON SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SFML glm json)
