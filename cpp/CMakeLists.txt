cmake_minimum_required(VERSION 3.16)
project(capitol_defense CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Use an installed raylib if there is one, otherwise download and build 5.5.
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(capitol_defense main.cpp)
target_link_libraries(capitol_defense PRIVATE raylib)
if(APPLE)
  target_link_libraries(capitol_defense PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
