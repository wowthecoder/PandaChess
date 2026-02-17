# PandaChess Engine

This folder contains the C++ UCI chess engine used by PandaChess.

## Current Implementation (as of this codebase)

- Bitboard board representation with mailbox + incremental Zobrist hash.
- Legal move generation (including castling, en passant, promotions).
- Magic-bitboard sliding attacks for bishops/rooks.
- Eval mode switch (`Eval`) with `NNUE`/`Handcrafted` modes (default `NNUE`).
- Tapered handcrafted evaluation (middlegame/endgame blend).
- Negamax alpha-beta search with iterative deepening.
- Transposition table, quiescence search, and common pruning/reduction heuristics.
- UCI protocol loop with configurable `Hash`, `Threads`, and `Eval`.
- Multi-threaded Lazy SMP search mode.

## Build

### Build + run tests

```bash
cd engine
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Release build (for GUI/cutechess)

```bash
cd engine
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binary output:

- `./build/panda-chess`

## Run As A UCI Engine

```bash
cd engine
./build/panda-chess
```

Minimal UCI session:

```text
uci
isready
position startpos moves e2e4 e7e5
go wtime 300000 btime 300000 winc 0 binc 0
stop
quit
```

## Supported UCI Commands

- `uci`
- `isready`
- `ucinewgame`
- `position startpos [moves ...]`
- `position fen <fen> [moves ...]`
- `go` with:
  - `wtime`, `btime`
  - `winc`, `binc`
  - `movetime`
  - `movestogo`
  - `depth`
  - `infinite`
- `stop`
- `setoption name Hash value <1..4096>`
- `setoption name Threads value <1..256>`
- `setoption name Eval value <NNUE|Handcrafted>`
- `quit`

## Search Overview

Main path:

1. Iterative deepening root search.
2. Aspiration windows around previous iteration score.
3. Negamax alpha-beta with PVS behavior at non-first moves.
4. Quiescence search at depth 0 (captures, or full evasions if in check).

Key move ordering:

- TT move
- Captures scored with SEE + MVV-LVA tie-break
- Killer moves
- History heuristic

Implemented pruning/reduction techniques:

- Delta pruning in quiescence
- Reverse futility pruning
- Futility pruning (shallow, quiet moves)
- Null move pruning (with verification at deeper nodes)
- Late move reductions (LMR)

Draw and terminal handling in search:

- Threefold repetition (via position hash history)
- Fifty-move rule
- Checkmate/stalemate detection

## Evaluation Overview

`eval.cpp` uses tapered MG/EG scoring with phase interpolation. Main terms include:

- Material
- Piece-square tables (PeSTO-style MG/EG PSTs)
- Pawn structure:
  - Passed pawns
  - Isolated pawns
  - Doubled pawns
- Bishop pair bonus
- Rook open/semi-open file bonuses
- Mobility (knight/bishop/rook/queen)
- King safety:
  - Pawn shield
  - King danger from attacking pieces in king zone

Returned score is from side-to-move perspective.

## Transposition Table Notes

- Single-entry buckets indexed by `hash & mask`.
- Replacement considers:
  - empty slot,
  - same key depth/flag quality,
  - staleness by generation,
  - depth/flag quality for collisions.
- `hashfull` is sampled occupancy reported in permille (UCI `info hashfull`).

## Code Map

- `main.cpp`: executable entry point.
- `uci.cpp`: UCI loop, command parsing, time management, search thread orchestration.
- `search.cpp/.h`: iterative deepening, negamax, quiescence, pruning, SMP.
- `eval.cpp/.h`: eval mode control + handcrafted tapered evaluation.
- `nnue.cpp/.h`: NNUE mode entry point / fallback wiring.
- `nnue/panda_nnue.cpp/.h`: active SF18 NNUE bridge + search-context incremental state wiring.
- `stockfish_src/nnue/`: Stockfish NNUE core used by the bridge implementation.
- `tt.cpp/.h`: transposition table.
- `movegen.cpp/.h`: legal move generation and perft.
- `attacks.cpp/.h`: attack tables and magic-bitboard sliders.
- `board.cpp/.h`: board state, make/unmake, FEN I/O, incremental hashing.
- `zobrist.cpp/.h`: Zobrist key initialization.
- `tests/`: unit and perft/search/eval tests.

## Estimate ELO With cutechess-cli

From `engine/`:

```bash
cutechess-cli \
  -engine cmd=./build/panda-chess name=Panda proto=uci \
  -engine cmd=/usr/games/stockfish name=Stockfish proto=uci option.UCI_LimitStrength=true option.UCI_Elo=1600 \
  -each tc=5+0.1 \
  -openings file=silversuite.pgn format=pgn order=random \
  -games 200 -repeat -recover
```

## Notes

- `tests/CMakeLists.txt` fetches GoogleTest automatically if it is not already available.
- `Eval=NNUE` loads both `nnue/sfnn_v10_big.nnue` and `nnue/sfnn_v10_small.nnue` (with source-dir and build-dir fallbacks). If either net is missing/invalid, it falls back to handcrafted evaluation.
- The NNUE backend uses Stockfish 18-style big/small networks and incremental accumulators inside search workers.
