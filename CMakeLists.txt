cmake_minimum_required(VERSION 3.24)
project(PinballFantasiesRemastered VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# pfr_core: everything that does not touch SDL or OpenGL. Unit-testable.
# ---------------------------------------------------------------------------
add_library(pfr_core STATIC
  src/core/Log.cpp
  src/core/File.cpp
  src/core/Png.cpp
  src/data/IffImage.cpp
  src/data/GameFiles.cpp
  src/assets/Exe.cpp
  src/core/Sha256.cpp
  src/data/GameVersion.cpp
  src/assets/TableAssets.cpp
  src/sound/Mod.cpp
  src/sound/Sequencer.cpp
  src/sound/Player.cpp
  src/game/Config.cpp
  src/intro/IntroAssets.cpp
  src/intro/Intro.cpp
  src/table/Table.cpp
  src/table/TablePhysics.cpp
  src/table/TableScript.cpp
  src/table/TableTasks.cpp
  src/table/TableGame.cpp
  src/table/Party.cpp
  src/table/Speed.cpp
  src/table/Show.cpp
  src/table/Stones.cpp
  src/gfx/Framebuffer.cpp
  src/gfx/Palette.cpp
)
target_include_directories(pfr_core PUBLIC src)
target_compile_options(pfr_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)

# ---------------------------------------------------------------------------
# Executable: SDL3 window, OpenGL renderer, audio device, game loop.
# ---------------------------------------------------------------------------
add_executable(pinball-fantasies MACOSX_BUNDLE
  src/main.cpp
  src/platform/Window.cpp
  src/platform/DataLocator.cpp
  src/platform/AudioDevice.cpp
  src/gfx/Renderer.cpp
  src/gfx/ShaderProgram.cpp
  src/game/App.cpp
)
target_link_libraries(pinball-fantasies PRIVATE pfr_core SDL3::SDL3 OpenGL::GL)
target_compile_definitions(pinball-fantasies PRIVATE GL_SILENCE_DEPRECATION=1 PFR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(pinball-fantasies PROPERTIES
  OUTPUT_NAME "Pinball Fantasies"
  MACOSX_BUNDLE_BUNDLE_NAME "Pinball Fantasies"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.pfr.pinball-fantasies"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

# Shaders travel with the bundle. This runs on every build, not only when the executable
# is relinked, so editing a shader alone still reaches the application.
add_custom_target(pfr_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/shaders"
          "$<TARGET_BUNDLE_CONTENT_DIR:pinball-fantasies>/Resources/shaders"
  DEPENDS pinball-fantasies
  COMMENT "Copying shaders into the application bundle")

# ---------------------------------------------------------------------------
# Command-line tools (asset inspection, no SDL).
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(pfr-tests
  tests/main.cpp
  tests/test_core.cpp
  tests/test_game.cpp
)
target_compile_definitions(pfr-tests PRIVATE PFR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(pfr-tests PRIVATE pfr_core)
add_test(NAME unit COMMAND pfr-tests)

add_executable(pfr-assets tools/assets_check.cpp)
target_link_libraries(pfr-assets PRIVATE pfr_core)

add_executable(pfr-play tools/play.cpp)
target_link_libraries(pfr-play PRIVATE pfr_core)
