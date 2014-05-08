cmake_minimum_required(VERSION 2.8.4)

include(ExternalProject)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (${CMAKE_GENERATOR} MATCHES ".* Makefiles$")
  set(MAKE_COMMAND "$(MAKE)")
else ()
  set(MAKE_COMMAND "make")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(V8_CXXFLAGS "-stdlib=libc++ -std=c++0x")
  set(V8_LDFLAGS "-stdlib=libc++")
else ()
  set(V8_CXXFLAGS "-std=c++0x")
  set(V8_LDFLAGS "")
endif ()

add_custom_target(v8 ${MAKE_COMMAND} dependencies
  COMMAND ${MAKE_COMMAND} LDFLAGS=${V8_LDFLAGS} CXXFLAGS=${V8_CXXFLAGS} GYPFLAGS="-Dmac_deployment_target=10.7" native
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/v8)

add_custom_target(libedit COMMAND ./autogen.sh
  COMMAND ./configure --enable-widec --enable-examples=no --enable-shared=no
  COMMAND ${MAKE_COMMAND}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/libedit)
