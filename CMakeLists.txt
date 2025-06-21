cmake_minimum_required(VERSION 3.14)
project(TruthTableExplorer LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(precompute precompute.cpp)
target_link_libraries(precompute 
  PRIVATE 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

add_executable(unrank unrank.cpp)
target_link_libraries(unrank 
  PRIVATE 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
