cmake_minimum_required(VERSION 3.14)
project(PandaChess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
option(PANDA_NNUE_AVX2 "Enable AVX2 path for Stockfish NNUE backend" OFF)

add_library(engine STATIC
    bitboard.cpp
    zobrist.cpp
    board.cpp
    attacks.cpp
    movegen.cpp
    eval.cpp
    nnue.cpp
    nnue/panda_nnue.cpp
    nnue/stockfish_shims.cpp
    stockfish_src/misc.cpp
    stockfish_src/bitboard.cpp
    stockfish_src/movegen.cpp
    stockfish_src/position.cpp
    stockfish_src/nnue/network.cpp
    stockfish_src/nnue/nnue_accumulator.cpp
    stockfish_src/nnue/features/half_ka_v2_hm.cpp
    stockfish_src/nnue/features/full_threats.cpp
    tt.cpp
    search.cpp
    uci.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(engine PRIVATE NNUE_EMBEDDING_OFF
    PANDA_ENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|X86_64|amd64|AMD64|i[3-6]86)$")
    target_compile_definitions(engine PRIVATE USE_SSE2)
    if(PANDA_NNUE_AVX2)
        target_compile_definitions(engine PRIVATE USE_AVX2 USE_SSE41 USE_SSSE3)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    target_compile_definitions(engine PRIVATE USE_NEON)
endif()

# UCI executable
find_package(Threads REQUIRED)
add_executable(panda-chess main.cpp)
target_link_libraries(panda-chess PRIVATE engine Threads::Threads)

add_subdirectory(tests)
