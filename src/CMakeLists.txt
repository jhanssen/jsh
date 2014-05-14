cmake_minimum_required(VERSION 2.8.6)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/node_modules/nan/
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/utf8/source
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/node/deps/v8/include
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/node/deps/uv/include)

add_subdirectory(node_modules)
