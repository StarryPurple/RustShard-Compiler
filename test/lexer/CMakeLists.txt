add_executable(lexer_gtest lexer_gtest.cpp)

target_link_libraries(lexer_gtest
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    frontend_lib
)

add_test(NAME lexer_ctest COMMAND $<TARGET_FILE:lexer_gtest>)