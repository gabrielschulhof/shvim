cmake_minimum_required(VERSION 3.12)
project(shvim)
add_executable(shvim src/main.c ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c)
add_custom_command(
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/make-mod-vim ${CMAKE_CURRENT_SOURCE_DIR}/src/mod.vim ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mod.vim
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c
  COMMENT "Compiling mod.vim"
)
target_link_libraries(shvim PUBLIC util)
