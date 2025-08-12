cmake_minimum_required(VERSION 3.20)
project(RustShardCompiler)

set(CMAKE_CXX_STANDARD 20)

include_directories(src/include)

file(GLOB SOURCE_FILES
    "main.cpp"
    "src/*.cpp" # in case
    "src/*/*.cpp"
)

message(STATUS "Source files found: ${SOURCE_FILES}")

add_executable(code ${SOURCE_FILES})