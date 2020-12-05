add_library(tk tk.c)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0)

pkg_check_modules(GOBJECT gobject-2.0)

target_include_directories(tk PUBLIC ${GLIB_INCLUDE_DIRS})
target_include_directories(tk PUBLIC ${GOBJECT_INCLUDE_DIRS})
target_link_libraries(tk PUBLIC ${GLIB_LIBRARIES})
target_link_libraries(tk PUBLIC ${GOBJECT_LIBRARIES})
