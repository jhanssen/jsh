cmake_minimum_required(VERSION 2.8.6)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()
add_definitions(-DOS_${CMAKE_SYSTEM_NAME} -DJSH_DOT_JS=\"${CMAKE_CURRENT_LIST_DIR}/jsh.js\")

include(${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct/compiler.cmake)

set(CLIENT_SOURCES
  Input.cpp
  Job.cpp
  NodeConnection.cpp
  Shell.cpp
  Splicer.cpp
  Util.cpp
  main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/duktape-0.9.0/src/duktape.c
  )

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct/include/rct
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/utf8/source
  )

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(SYSTEM_LIBS iconv)
endif ()

add_subdirectory(node_modules)
