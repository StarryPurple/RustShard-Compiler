cmake_minimum_required(VERSION 3.20)
project(RustShardCompiler)

set(CMAKE_CXX_STANDARD 20)

include_directories(src/include)

# end to end

file(GLOB SOURCE_FILES
    "main.cpp"
    "src/*.cpp" # in case
    "src/*/*.cpp"
)
message(STATUS "Source files found: ${SOURCE_FILES}")
add_executable(code ${SOURCE_FILES})

# gtest
find_package(GTest REQUIRED)

add_library(lexer src/include/frontend/lexer.h src/frontend/lexer.cpp)
add_executable(lexer_gtest test/lexer/lexer_gtest.cpp)
set_target_properties(lexer_gtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/lexer")
target_link_libraries(lexer_gtest PRIVATE GTest::gtest_main GTest::gtest)
target_link_libraries(lexer_gtest PRIVATE lexer)