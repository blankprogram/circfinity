cmake_minimum_required(VERSION 3.21)
project(circfinity-wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD        23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS      OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS filesystem system multiprecision)

add_library(compute_lib STATIC
  src/compute.cpp
)
target_include_directories(compute_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(compute_lib PUBLIC
  Boost::filesystem
  Boost::system
  Boost::multiprecision
)

add_executable(wasm_main
  src/wasm_main.cpp
)
target_link_libraries(wasm_main PRIVATE compute_lib)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_compile_options(wasm_main PRIVATE -Wall -Wextra)
  target_link_options(wasm_main PRIVATE
    "--bind"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createModule"
  )
  set_target_properties(wasm_main PROPERTIES SUFFIX ".js")
endif()

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
