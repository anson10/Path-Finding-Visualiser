cmake_minimum_required(VERSION 3.25)
project(pathfinder VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PATHFINDER_BUILD_APP "Build the SFML visualiser (needs SFML 2.5+)" ON)
option(PATHFINDER_BUILD_TESTS "Build the Catch2 tests" ON)
option(PATHFINDER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PATHFINDER_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

# Shared compile settings for every target in this project.
add_library(pathfinder_options INTERFACE)
if(MSVC)
  target_compile_options(pathfinder_options INTERFACE /W4 $<$<BOOL:${PATHFINDER_WARNINGS_AS_ERRORS}>:/WX>)
else()
  target_compile_options(pathfinder_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                         -Wno-sign-conversion $<$<BOOL:${PATHFINDER_WARNINGS_AS_ERRORS}>:-Werror>)
  if(PATHFINDER_SANITIZE)
    target_compile_options(pathfinder_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(pathfinder_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()

# The search library: no graphics, no I/O.
add_library(pathfinder src/grid.cpp src/search.cpp)
target_include_directories(pathfinder PUBLIC include)
target_link_libraries(pathfinder PRIVATE pathfinder_options)

add_executable(pathfinder-bench bench/bench.cpp)
target_link_libraries(pathfinder-bench PRIVATE pathfinder pathfinder_options)

if(PATHFINDER_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1
                       GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(Catch2)
  enable_testing()
  add_executable(pathfinder-tests tests/test_search.cpp)
  target_link_libraries(pathfinder-tests PRIVATE pathfinder pathfinder_options Catch2::Catch2WithMain)
  include(Catch)
  catch_discover_tests(pathfinder-tests)
endif()

if(PATHFINDER_BUILD_APP)
  find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
  find_package(OpenGL QUIET)
  if(SFML_FOUND AND OpenGL_FOUND)
    include(FetchContent)
    # Dear ImGui for the side panel, through its SFML backend. SYSTEM: their headers are not
    # held to this project's warning flags.
    FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.89.9
                         GIT_SHALLOW TRUE SYSTEM)
    FetchContent_MakeAvailable(imgui)
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
    set(IMGUI_SFML_FIND_SFML OFF)
    FetchContent_Declare(imgui-sfml GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git GIT_TAG v2.6.1
                         GIT_SHALLOW TRUE SYSTEM)
    FetchContent_MakeAvailable(imgui-sfml)

    add_executable(pathfinder-app app/main.cpp app/model.cpp app/scene.cpp app/panel.cpp)
    target_link_libraries(pathfinder-app PRIVATE pathfinder pathfinder_options ImGui-SFML::ImGui-SFML
                          sfml-graphics sfml-window sfml-system)
    target_compile_definitions(pathfinder-app PRIVATE PATHFINDER_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
  else()
    message(STATUS "SFML or OpenGL not found: skipping pathfinder-app (library, tests and benchmark still build)")
  endif()
endif()
