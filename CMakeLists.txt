cmake_minimum_required(VERSION 2.8.4)

project(jsh)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(NODE_DIR ${PROJECT_BINARY_DIR}/3rdparty/node/)
set(NODE_GYP ${NODE_DIR}/deps/npm/node_modules/node-gyp/bin/node-gyp.js)
set(NODE_BIN ${NODE_DIR}/node)

add_subdirectory(3rdparty)
add_subdirectory(src)

add_custom_target(release
    COMMAND cmake -DCMAKE_BUILD_TYPE=Release .
    WORKING_DIRECTORY .)

add_custom_target(debug
    COMMAND cmake -DCMAKE_BUILD_TYPE=Debug .
    WORKING_DIRECTORY .)
