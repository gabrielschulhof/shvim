cmake_minimum_required(VERSION 3.12)
project(shvim)
add_executable(shvim src/main.c)
target_link_libraries(shvim PUBLIC util)
