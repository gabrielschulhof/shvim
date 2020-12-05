cmake_minimum_required(VERSION 3.12)

project(editor)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0)

pkg_check_modules(GOBJECT gobject-2.0)

add_subdirectory(tk)

add_executable(editor src/main.c)

add_compile_definitions(editor ${GTK2_CFLAGS_OTHER})

target_include_directories(editor PUBLIC ${GLIB_INCLUDE_DIRS})
target_include_directories(editor PUBLIC ${GOBJECT_INCLUDE_DIRS})
target_include_directories(editor PUBLIC "${PROJECT_SOURCE_DIR}/tk/include")

target_link_libraries(editor PUBLIC tk)
target_link_libraries(editor PUBLIC ${GLIB_LIBRARIES})
target_link_libraries(editor PUBLIC ${GOBJECT_LIBRARIES})
