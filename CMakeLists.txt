cmake_minimum_required(VERSION 3.5.1)
project(shvim)
add_executable(shvim src/main.c src/debug.c src/vi.c ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c)
add_custom_command(
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/make-mod-vim ${CMAKE_CURRENT_SOURCE_DIR}/src/mod.vim ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mod.vim
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mod.vim.c
  COMMENT "Compiling mod.vim"
)
target_link_libraries(shvim PUBLIC util)
target_include_directories(shvim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
