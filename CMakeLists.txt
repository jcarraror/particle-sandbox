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

option(PARTICLE_SANDBOX_ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

function(particle_sandbox_apply_common_warnings target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(particle_sandbox_enable_coverage target_name)
  if (NOT PARTICLE_SANDBOX_ENABLE_COVERAGE)
    return()
  endif()
  if (MSVC)
    message(WARNING "Coverage instrumentation is only configured for GCC/Clang-like toolchains")
    return()
  endif()
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
    target_link_options(${target_name} PRIVATE --coverage)
  else()
    message(WARNING "Coverage instrumentation unsupported for compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()

add_executable(particle_sandbox
  src/main.cpp
  src/world.cpp
  src/world_thermo.cpp
  src/world_pressure.cpp
  src/world_pressure_gas.cpp
  src/world_pressure_dense.cpp
  src/world_rules.cpp
  src/render_core.cpp
  src/render_font.cpp
  src/render_sdl.cpp
)

target_include_directories(particle_sandbox PRIVATE src)
particle_sandbox_apply_common_warnings(particle_sandbox)
particle_sandbox_enable_coverage(particle_sandbox)

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
      tests/world/test_world_invariants.cpp
      tests/world/test_world_core_whitebox.cpp
      tests/world/test_world_rules_whitebox.cpp
      tests/world/test_world_thermo_whitebox.cpp
      tests/world/test_world_pressure_whitebox.cpp
      tests/render/test_render_sdl_pure.cpp
      tests/render/test_render_sdl_integration.cpp
      src/world.cpp
      src/world_thermo.cpp
      src/world_pressure.cpp
      src/world_pressure_gas.cpp
      src/world_pressure_dense.cpp
      src/world_rules.cpp
    )
    target_include_directories(particle_sandbox_tests PRIVATE src)
    target_link_libraries(particle_sandbox_tests PRIVATE ${PARTICLE_SANDBOX_CATCH2_TARGET})
    particle_sandbox_apply_common_warnings(particle_sandbox_tests)
    particle_sandbox_enable_coverage(particle_sandbox_tests)
    add_test(NAME particle_sandbox_tests COMMAND particle_sandbox_tests)

    if (PARTICLE_SANDBOX_ENABLE_COVERAGE)
      find_program(GCOVR_EXECUTABLE gcovr)
      if (GCOVR_EXECUTABLE)
        set(PARTICLE_SANDBOX_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
        set(PARTICLE_SANDBOX_COVERAGE_HTML "${PARTICLE_SANDBOX_COVERAGE_DIR}/index.html")
        add_custom_target(coverage
          COMMAND ${CMAKE_COMMAND} -E rm -rf "${PARTICLE_SANDBOX_COVERAGE_DIR}"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${PARTICLE_SANDBOX_COVERAGE_DIR}"
          # Remove stale coverage artifacts (e.g. after file moves/renames) so gcovr
          # doesn't try to resolve deleted source paths.
          COMMAND find "${CMAKE_BINARY_DIR}" -name "*.gcda" -delete
          COMMAND find "${CMAKE_BINARY_DIR}" -name "*.gcov" -delete
          COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
          COMMAND ${GCOVR_EXECUTABLE}
            -r "${CMAKE_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}"
            --filter "${CMAKE_SOURCE_DIR}/src"
            --exclude "${CMAKE_SOURCE_DIR}/tests"
          COMMAND ${GCOVR_EXECUTABLE}
            -r "${CMAKE_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}"
            --filter "${CMAKE_SOURCE_DIR}/src"
            --exclude "${CMAKE_SOURCE_DIR}/tests"
            --html
            --html-details
            -o "${PARTICLE_SANDBOX_COVERAGE_HTML}"
          WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
          DEPENDS particle_sandbox_tests
          USES_TERMINAL
          VERBATIM
        )
        message(STATUS "Coverage target enabled: cmake --build ${CMAKE_BINARY_DIR} --target coverage")
      else()
        message(STATUS "gcovr not found: skipping coverage target")
      endif()
    endif()
  else()
    message(STATUS "Catch2 not found: skipping particle_sandbox_tests target")
  endif()
endif()

find_package(SDL2 CONFIG QUIET)

if (SDL2_FOUND)
  target_link_libraries(particle_sandbox PRIVATE SDL2::SDL2 SDL2::SDL2main)
  if (TARGET particle_sandbox_tests)
    target_sources(particle_sandbox_tests PRIVATE src/render_sdl.cpp)
    target_sources(particle_sandbox_tests PRIVATE src/render_core.cpp)
    target_sources(particle_sandbox_tests PRIVATE src/render_font.cpp)
    target_link_libraries(particle_sandbox_tests PRIVATE SDL2::SDL2 SDL2::SDL2main)
  endif()
else()
  find_package(SDL2 REQUIRED)
  target_include_directories(particle_sandbox PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(particle_sandbox PRIVATE ${SDL2_LIBRARIES})
  if (TARGET particle_sandbox_tests)
    target_sources(particle_sandbox_tests PRIVATE src/render_sdl.cpp)
    target_sources(particle_sandbox_tests PRIVATE src/render_core.cpp)
    target_sources(particle_sandbox_tests PRIVATE src/render_font.cpp)
    target_include_directories(particle_sandbox_tests PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(particle_sandbox_tests PRIVATE ${SDL2_LIBRARIES})
  endif()
endif()
