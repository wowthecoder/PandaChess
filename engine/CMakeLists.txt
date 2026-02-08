cmake_minimum_required(VERSION 3.14)
project(PandaChess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)
