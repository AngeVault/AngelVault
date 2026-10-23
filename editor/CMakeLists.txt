add_executable(AngeVaultEditor WIN32 
    src/main.cpp 
    src/sfml_widget.h 
    src/sfml_widget.cpp
    "src/customWindow.h"
    "src/customWindow.cpp"
)

target_link_libraries(AngeVaultEditor PRIVATE Qt6::Widgets AngeVault::Render2D)

if(WIN32)
  find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(WINDEPLOYQT)
    add_custom_command(TARGET AngeVaultEditor POST_BUILD
      COMMAND ${WINDEPLOYQT} --no-compiler-runtime $<TARGET_FILE:AngeVaultEditor>)
  endif()
endif()
