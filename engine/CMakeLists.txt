cmake_minimum_required(VERSION 3.14)
project(PandaChess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_library(engine STATIC
    bitboard.cpp
    zobrist.cpp
    board.cpp
    attacks.cpp
    movegen.cpp
    eval.cpp
    tt.cpp
    search.cpp
    uci.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# UCI executable
find_package(Threads REQUIRED)
add_executable(panda-chess main.cpp)
target_link_libraries(panda-chess PRIVATE engine Threads::Threads)

add_subdirectory(tests)
