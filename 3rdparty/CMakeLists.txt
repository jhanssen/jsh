cmake_minimum_required(VERSION 2.8.4)

include(ExternalProject)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(rct)

if (${CMAKE_GENERATOR} MATCHES ".* Makefiles$")
  set(MAKE_COMMAND "$(MAKE)")
else ()
  set(MAKE_COMMAND "make")
endif()

add_custom_target(v8 ${MAKE_COMMAND} dependencies
		  COMMAND ${MAKE_COMMAND} native
		  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/v8
                  )

add_custom_target(libedit ./configure --enable-widec --enable-examples=no
                  COMMAND ${MAKE_COMMAND}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/libedit
                  )
