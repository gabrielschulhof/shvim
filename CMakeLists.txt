cmake_minimum_required(VERSION 3.12)

project(editor)

find_package(PkgConfig REQUIRED)
include(FindCurses)

add_executable(editor src/main.c)
target_link_libraries(editor PUBLIC util)
