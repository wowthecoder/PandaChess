add_library(engine STATIC
    bitboard.cpp
    zobrist.cpp
    board.cpp
    attacks.cpp
    movegen.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)
