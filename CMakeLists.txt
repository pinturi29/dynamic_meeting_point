cmake_minimum_required(VERSION 3.20)
project(midpoint_finder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(midpoint_finder main.cpp)

target_link_libraries(midpoint_finder PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)
