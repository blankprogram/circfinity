cmake_minimum_required(VERSION 3.14)
project(TruthTableExplorer LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Choose the type of build (Debug, Release, etc.)" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -march=native -flto -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE
    "-Wl,--gc-sections -fuse-ld=gold")

add_executable(precompute precompute.cpp)
add_executable(unrank      unrank.cpp)
add_executable(compute compute.cpp)
