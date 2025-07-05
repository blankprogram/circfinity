cmake_minimum_required(VERSION 3.21)
project(circfinity-wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)

option(BUILD_TESTS "Enable building tests" ON)

# Optional: use ccache
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# ------------------------------
# Compute logic library
# ------------------------------
add_library(compute_lib STATIC
  src/compute.cpp
)
target_include_directories(compute_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# ------------------------------
# Emscripten-specific WASM target
# ------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_executable(wasm_main
    src/wasm_main.cpp
  )
  target_link_libraries(wasm_main PRIVATE compute_lib)
  target_compile_options(wasm_main PRIVATE -Wall -Wextra)
  target_link_options(wasm_main PRIVATE
    "--bind" "-sMODULARIZE=1" "-sEXPORT_NAME=createModule"
  )
  set_target_properties(wasm_main PROPERTIES SUFFIX ".js")

  add_custom_command(TARGET wasm_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${CMAKE_SOURCE_DIR}/../frontend/public
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:wasm_main>
      ${CMAKE_SOURCE_DIR}/../frontend/public/wasm_main.js
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_BINARY_DIR}/wasm_main.wasm
      ${CMAKE_SOURCE_DIR}/../frontend/public/wasm_main.wasm
  )
endif()

# ------------------------------
# Optional test target
# ------------------------------
if (BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
