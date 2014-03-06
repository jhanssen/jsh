cmake_minimum_required(VERSION 2.8.6)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

include(${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct/compiler.cmake)

set(CLIENT_SOURCES
  Shell.cpp
  main.cpp
  )

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rct/include/rct
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/libedit/src
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/v8/include
  )

link_directories(
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/libedit/src/.libs
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/v8/out/native
  ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/v8/out/native/obj.host/third_party/icu
  )

add_executable(jsh ${CLIENT_SOURCES})
target_link_libraries(jsh pthread termcap rct edit v8_base.x64 v8_nosnapshot.x64 icui18n icuuc icudata)

#add_dependencies(jsh v8)
