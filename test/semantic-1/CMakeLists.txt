add_executable(semantic_test_1 semantic-1_gtest.cpp)

target_link_libraries(semantic_test_1
    PRIVATE
    GTest::gtest_main
    GTest::gtest
    frontend_lib
)

set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testcases")

target_compile_definitions(semantic_test_1 PRIVATE
    TEST_DIRECTORY_PATH="${TEST_DIR}"
)

add_test(
    NAME semantic_1_test_suite
    COMMAND $<TARGET_FILE:semantic_test_1>
)