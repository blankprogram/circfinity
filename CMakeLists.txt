cmake_minimum_required(VERSION 3.14)
project(circfinity LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

add_library(u128 INTERFACE)
target_include_directories(u128 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(compute_lib src/compute.cpp)
target_include_directories(compute_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(compute_lib PRIVATE u128)

add_executable(wasm_main src/wasm_main.cpp)
target_link_libraries(wasm_main PRIVATE compute_lib u128)

set_property(TARGET wasm_main PROPERTY LINK_FLAGS
  "--bind -s MODULARIZE=1 -s EXPORT_NAME=createModule"
)

set_target_properties(wasm_main PROPERTIES SUFFIX ".js")


add_custom_command(TARGET wasm_main POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend/public

  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_main.js
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend/public/wasm_main.js

  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/wasm_main.wasm
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend/public/wasm_main.wasm
)
