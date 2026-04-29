cmake_minimum_required(VERSION 3.21)
project(InterPlanetaryVoxelDuel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/CMakeLists.txt")
  add_subdirectory(third_party/raylib)
else()
  include(FetchContent)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
  )
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(interplanetary
  src/main.cpp
  src/World.cpp
  src/Game.cpp
)

target_include_directories(interplanetary PRIVATE src)
target_link_libraries(interplanetary PRIVATE raylib)

if(APPLE)
  target_link_libraries(interplanetary PRIVATE
    "-framework IOKit"
    "-framework Cocoa"
    "-framework OpenGL"
  )
endif()
