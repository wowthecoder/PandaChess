add_executable(test_board test_board.cpp)
target_link_libraries(test_board PRIVATE engine)

add_test(NAME test_board COMMAND test_board)
