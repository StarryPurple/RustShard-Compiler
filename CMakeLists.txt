cmake_minimum_required(VERSION 3.20)
project(RustShardCompiler)

set(CMAKE_CXX_STANDARD 20)

# end to end
add_executable(main)

target_sources(main
    PRIVATE
    main.cpp
    src/frontend/ast.cpp
    src/frontend/ast_type.cpp
    src/frontend/lexer.cpp
    src/frontend/parser.cpp
)
target_include_directories(main
    PRIVATE
    src/include/common
    src/include/frontend
)


# gtest
find_package(GTest REQUIRED)

add_library(lexer src/frontend/lexer.cpp)
target_include_directories(lexer PUBLIC src/include/frontend)
add_executable(lexer_gtest test/lexer/lexer_gtest.cpp)
target_link_libraries(lexer_gtest PRIVATE GTest::gtest_main GTest::gtest)
target_link_libraries(lexer_gtest PRIVATE lexer)
set_target_properties(lexer_gtest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/lexer")

# ctest
enable_testing()

add_test(NAME lexer_ctest COMMAND $<TARGET_FILE:lexer_gtest>)