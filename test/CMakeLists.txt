enable_testing()

find_package(GTest REQUIRED)

add_subdirectory(lexer)
add_subdirectory(ast)
add_subdirectory(semantic)
add_subdirectory(IR)