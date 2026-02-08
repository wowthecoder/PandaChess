find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(test_board test_board.cpp)
add_executable(test_movegen test_movegen.cpp)
add_executable(test_search test_search.cpp)

if(TARGET GTest::gtest)
    target_link_libraries(test_board PRIVATE engine GTest::gtest)
    target_link_libraries(test_movegen PRIVATE engine GTest::gtest)
    target_link_libraries(test_search PRIVATE engine GTest::gtest)
elseif(TARGET GTest::GTest)
    target_link_libraries(test_board PRIVATE engine GTest::GTest)
    target_link_libraries(test_movegen PRIVATE engine GTest::GTest)
    target_link_libraries(test_search PRIVATE engine GTest::GTest)
else()
    message(FATAL_ERROR "Could not find a GoogleTest target to link")
endif()

include(GoogleTest)
gtest_discover_tests(test_board)
gtest_discover_tests(test_movegen)
gtest_discover_tests(test_search)
