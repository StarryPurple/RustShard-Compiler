add_executable(ast_type_gtest type_gtest.cpp)

target_link_libraries(ast_type_gtest
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    frontend_lib
)

add_test(NAME ast_type_ctest COMMAND $<TARGET_FILE:ast_type_gtest>)