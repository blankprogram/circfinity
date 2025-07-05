cmake_minimum_required(VERSION 3.21)
project(circfinity-wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)

option(BUILD_TESTS "Enable building tests" ON)

# Optional: ccache
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# ---- Find Boost.Multiprecision via vcpkg
find_package(boost_multiprecision CONFIG REQUIRED)

# ---- Compute library
add_library(compute_lib STATIC
  src/compute.cpp
)
target_include_directories(compute_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(compute_lib PUBLIC Boost::multiprecision)

# ---- Emscripten executable
if (DEFINED ENV{EMSCRIPTEN} OR CMAKE_CXX_COMPILER MATCHES "em\\+\\+")
  message(STATUS "Forcing Emscripten build section")

  add_executable(wasm_main src/wasm_main.cpp)
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

# ---- Optional test support
if (BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
