cmake_minimum_required(VERSION 3.20)
project(RustShardCompiler)

set(CMAKE_CXX_STANDARD 20)

add_library(frontend_lib STATIC
    src/frontend/type.cpp
    src/frontend/lexer.cpp
    src/frontend/parser.cpp
    src/frontend/recursive_visitor.cpp
    src/frontend/syntax_check.cpp
    src/frontend/defs.cpp
    src/frontend/constant.cpp
    src/frontend/IR_generator.cpp
)

target_include_directories(frontend_lib PUBLIC
    src/include/common
    src/include/frontend
    src/template/frontend
)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE frontend_lib)

enable_testing()
add_subdirectory(test)