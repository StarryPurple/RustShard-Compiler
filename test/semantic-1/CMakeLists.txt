add_executable(semantic_test_1 semantic-1-test.cpp)

target_link_libraries(semantic_test_1 PRIVATE frontend_lib)

add_test(
    NAME semantic_1_ctest
    COMMAND $<TARGET_FILE:semantic_test_1> .
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)