
cmake_minimum_required(VERSION 3.14)
project(TruthTableExplorer LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(circfinity main.cpp)
target_link_libraries(circfinity OpenSSL::Crypto)

