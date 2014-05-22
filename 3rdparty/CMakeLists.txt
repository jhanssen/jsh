cmake_minimum_required(VERSION 2.8.4)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (${CMAKE_GENERATOR} MATCHES ".* Makefiles$")
  set(MAKE_COMMAND "$(MAKE)")
else ()
  set(MAKE_COMMAND "make")
endif()

add_custom_target(node
  COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/node
  COMMAND ${MAKE_COMMAND} -C ${CMAKE_CURRENT_BINARY_DIR}/node
  # COMMAND ninja -C ${CMAKE_CURRENT_BINARY_DIR}/node/out/Release/
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/node)

add_custom_target(readline
  COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/readline
  COMMAND ${MAKE_COMMAND} -C ${CMAKE_CURRENT_BINARY_DIR}/readline
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/readline)
