cmake_minimum_required(VERSION 2.8.4)

project(jsh)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(3rdparty)
add_subdirectory(src)

add_custom_target(release
    COMMAND cmake -DCMAKE_BUILD_TYPE=Release .
    WORKING_DIRECTORY .)

add_custom_target(debug
    COMMAND cmake -DCMAKE_BUILD_TYPE=Debug .
    WORKING_DIRECTORY .)
