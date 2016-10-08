cmake_minimum_required(VERSION 3.0)
project(SimpleON)

add_library(simpleon STATIC simpleon.cpp)

add_executable(simpleon_parse simpleon_parse.cpp)
target_link_libraries(simpleon_parse simpleon)