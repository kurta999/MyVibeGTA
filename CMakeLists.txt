cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(MiniCity3D LANGUAGES CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JoltPhysics/Build/CMakeLists.txt")
  message(FATAL_ERROR "Jolt Physics v5.6.0 is required in third_party/JoltPhysics")
endif()
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Preserve the game's compiler flags" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "Use the same static MSVC runtime as the game" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Keep Jolt builds straightforward" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "No Jolt install target" FORCE)
add_subdirectory(third_party/JoltPhysics/Build third_party/JoltPhysics-build)

set(COMMON_SOURCES main.cpp input.cpp logging.cpp game.cpp ai.cpp content.cpp audio.cpp ui.cpp physics.cpp weapons.cpp
    savegame.cpp camera.cpp props.cpp fire.cpp police.cpp commerce.cpp traversal.cpp weather.cpp regions.cpp)
list(TRANSFORM COMMON_SOURCES PREPEND "src/")
add_executable(MiniCity3D WIN32 ${COMMON_SOURCES}
    src/renderer_dx11.cpp src/dx11_scene.cpp src/dx11_assets.cpp src/dx11_hud.cpp
    src/jolt_world.cpp src/debug_menu.cpp)
target_compile_features(MiniCity3D PRIVATE cxx_std_17)
target_compile_definitions(MiniCity3D PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MINI_CITY_JOLT)
target_include_directories(MiniCity3D PRIVATE third_party/JoltPhysics)
target_link_libraries(MiniCity3D PRIVATE Jolt user32 gdi32 gdiplus ole32 xaudio2 d3d11 dxgi d3dcompiler)
add_custom_command(TARGET MiniCity3D POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MiniCity3D>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:MiniCity3D>/data")

option(MINI_CITY_BUILD_OPENGL "Build the old OpenGL fallback executable" OFF)
if(MINI_CITY_BUILD_OPENGL)
  add_executable(MiniCity3DGL WIN32 ${COMMON_SOURCES} src/renderer.cpp src/textures.cpp)
  target_compile_features(MiniCity3DGL PRIVATE cxx_std_17)
  target_compile_definitions(MiniCity3DGL PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(MiniCity3DGL PRIVATE user32 gdi32 opengl32 glu32 gdiplus ole32 xaudio2)
  add_custom_command(TARGET MiniCity3DGL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MiniCity3DGL>/assets")
endif()

include(CTest)
if(BUILD_TESTING)
  set(SIMULATION_SOURCES game.cpp input.cpp ai.cpp content.cpp physics.cpp props.cpp savegame.cpp
      weapons.cpp camera.cpp audio.cpp ui.cpp fire.cpp police.cpp commerce.cpp traversal.cpp
      weather.cpp regions.cpp jolt_world.cpp debug_menu.cpp)
  list(TRANSFORM SIMULATION_SOURCES PREPEND "src/")
  add_executable(simulation_smoke tests/simulation_smoke.cpp ${SIMULATION_SOURCES})
  target_compile_features(simulation_smoke PRIVATE cxx_std_17)
  target_compile_definitions(simulation_smoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MINI_CITY_JOLT)
  target_include_directories(simulation_smoke PRIVATE third_party/JoltPhysics)
  target_link_libraries(simulation_smoke PRIVATE Jolt user32 gdi32 opengl32 ole32 xaudio2)
  add_custom_command(TARGET simulation_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:simulation_smoke>/data")
  add_test(NAME simulation_smoke COMMAND simulation_smoke)
  set_tests_properties(simulation_smoke PROPERTIES
      WORKING_DIRECTORY "$<TARGET_FILE_DIR:simulation_smoke>")
  add_executable(asset_smoke tests/asset_smoke.cpp src/dx11_assets.cpp)
  target_compile_features(asset_smoke PRIVATE cxx_std_17)
  add_test(NAME asset_smoke COMMAND asset_smoke)
  set_tests_properties(asset_smoke PROPERTIES
      WORKING_DIRECTORY "$<TARGET_FILE_DIR:MiniCity3D>")
endif()
