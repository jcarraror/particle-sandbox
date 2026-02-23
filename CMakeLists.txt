#[[
  @file CMakeLists.txt
  @brief CMake build definition for the Particle Sandbox executable.
]]

cmake_minimum_required(VERSION 3.25)
project(particle_sandbox LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(particle_sandbox
  src/main.cpp
  src/world.cpp
  src/world_thermo.cpp
  src/world_pressure.cpp
  src/world_pressure_gas.cpp
  src/world_pressure_dense.cpp
  src/world_rules.cpp
  src/render_sdl.cpp
)

target_include_directories(particle_sandbox PRIVATE src)

if (MSVC)
  target_compile_options(particle_sandbox PRIVATE /W4 /permissive-)
else()
  target_compile_options(particle_sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (BUILD_TESTING)
  find_package(Catch2 3 QUIET CONFIG)
  if (Catch2_FOUND)
    set(PARTICLE_SANDBOX_CATCH2_TARGET Catch2::Catch2)
  else()
    find_package(Catch2 2 QUIET CONFIG)
    if (Catch2_FOUND)
      set(PARTICLE_SANDBOX_CATCH2_TARGET Catch2::Catch2)
    endif()
  endif()

  if (Catch2_FOUND)
    add_executable(particle_sandbox_tests
      tests/test_world_invariants.cpp
      src/world.cpp
      src/world_thermo.cpp
      src/world_pressure.cpp
      src/world_pressure_gas.cpp
      src/world_pressure_dense.cpp
      src/world_rules.cpp
    )
    target_include_directories(particle_sandbox_tests PRIVATE src)
    target_link_libraries(particle_sandbox_tests PRIVATE ${PARTICLE_SANDBOX_CATCH2_TARGET})
    if (MSVC)
      target_compile_options(particle_sandbox_tests PRIVATE /W4 /permissive-)
    else()
      target_compile_options(particle_sandbox_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME particle_sandbox_tests COMMAND particle_sandbox_tests)
  else()
    message(STATUS "Catch2 not found: skipping particle_sandbox_tests target")
  endif()
endif()

find_package(SDL2 CONFIG QUIET)

if (SDL2_FOUND)
  target_link_libraries(particle_sandbox PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
  find_package(SDL2 REQUIRED)
  target_include_directories(particle_sandbox PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(particle_sandbox PRIVATE ${SDL2_LIBRARIES})
endif()
