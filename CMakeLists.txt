cmake_minimum_required(VERSION 3.20)
project(RustShardCompiler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -Wall -Wextra")

add_library(builtin_lib STATIC
    builtin/builtin.c
)

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

add_executable(compiler src/main.cpp)
target_link_libraries(compiler PRIVATE frontend_lib builtin_lib)

get_filename_component(BUILTIN_C_ABSPATH "${CMAKE_SOURCE_DIR}/builtin/builtin.c" ABSOLUTE)
string(REPLACE "\\" "\\\\" BUILTIN_C_ESCAPED "${BUILTIN_C_ABSPATH}")
target_compile_definitions(compiler PRIVATE
    BUILTIN_C_PATH="${BUILTIN_C_ESCAPED}"
)

enable_testing()
add_subdirectory(test)