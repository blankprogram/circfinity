cmake_minimum_required(VERSION 3.14)
project(circfinity LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

add_library(u128 INTERFACE)
target_include_directories(u128 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_options(u128 INTERFACE
  $<$<CONFIG:Release>:-O3;-march=native>
)
target_link_options(u128 INTERFACE
  $<$<CONFIG:Release>:-flto>
)

add_executable(compute src/compute.cpp)
target_link_libraries(compute PRIVATE u128)
target_include_directories(compute PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Catch2 3 REQUIRED)

add_executable(test_u128 tests/test_uint128.cpp)
target_link_libraries(test_u128
  PRIVATE
    u128
    Catch2::Catch2WithMain
)
target_include_directories(test_u128 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME uint128_tests COMMAND test_u128)

