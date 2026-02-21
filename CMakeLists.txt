#[[
  @file CMakeLists.txt
  @brief CMake build definition for the Particle Sandbox executable.
]]

cmake_minimum_required(VERSION 3.25)
project(particle_sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(particle_sandbox
  src/main.cpp
  src/world.cpp
  src/render_sdl.cpp
)

target_include_directories(particle_sandbox PRIVATE src)

if (MSVC)
  target_compile_options(particle_sandbox PRIVATE /W4 /permissive-)
else()
  target_compile_options(particle_sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(SDL2 CONFIG QUIET)

if (SDL2_FOUND)
  target_link_libraries(particle_sandbox PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
  find_package(SDL2 REQUIRED)
  target_include_directories(particle_sandbox PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(particle_sandbox PRIVATE ${SDL2_LIBRARIES})
endif()
