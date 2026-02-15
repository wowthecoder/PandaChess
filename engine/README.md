# PandaChess Engine Guide

This document explains the engine layout, what each core file does, and how the search algorithm is implemented component-by-component.

## High-Level Architecture

The engine is split into these layers:

1. Core chess primitives and data types
2. Board state and hashing
3. Attack generation and legal move generation
4. Evaluation
5. Transposition table
6. Search (iterative deepening, aspiration windows, alpha-beta/negamax, quiescence, pruning/reductions)

Primary flow:

1. `search()` / `searchDepth()` in `search.cpp` chooses depth/time strategy.
2. `searchRoot()` enumerates and orders root moves.
3. `negamax()` recursively searches with alpha-beta and pruning.
4. At leaf depth, `quiescence()` extends tactical captures.
5. `evaluate()` scores quiet positions.

## File-by-File Guide

### `types.h`

Defines the foundational enums and helpers:

- `Color`, `PieceType`, `Piece`, `Square`, `CastlingRights`
- Bitwise helpers for castling flags
- Mapping helpers:
  - `make_piece(Color, PieceType)`
  - `piece_color(Piece)`
  - `piece_type(Piece)`
  - `square_file(Square)`, `square_rank(Square)`, `make_square(file, rank)`

This file is shared by almost all engine modules.

### `bitboard.h` / `bitboard.cpp`

Bitboard utilities:

- `using Bitboard = uint64_t`
- Precomputed masks:
  - `RankMask[8]`
  - `FileMask[8]`
- Core bit ops:
  - `square_bb()`
  - `popcount()`
  - `lsb()`
  - `pop_lsb()`
- Debug helper in `bitboard.cpp`:
  - `print_bitboard()`

These utilities are used heavily by attacks, move generation, eval, and search helpers.

### `move.h`

Defines compact 16-bit move encoding and move containers:

- `Move` type and `NullMove`
- `MoveType`: `Normal`, `Promotion`, `EnPassant`, `Castling`
- Move builders/parsers:
  - `make_move()`, `make_promotion()`
  - `move_from()`, `move_to()`, `move_type()`, `promotion_type()`
- UCI formatter: `move_to_uci()`
- `MoveList` fixed-size array container used during generation/search

### `board.h` / `board.cpp`

Represents complete chess state and incremental updates.

State representation:

- Piece bitboards: `pieceBB[12]`
- Occupancy bitboards: white/black/all in `occupancy[3]`
- Mailbox board: `mailbox[64]`
- Side to move, castling rights, en-passant square
- Halfmove/fullmove counters
- Incremental Zobrist hash key

Core logic in `board.cpp`:

- FEN input/output:
  - `set_fen()`
  - `to_fen()`
- Piece placement/removal with hash updates:
  - `put_piece()`
  - `remove_piece()`
- Full hash recomputation:
  - `compute_hash()` (useful for validation against incremental hash)
- Attack query:
  - `is_square_attacked()`
- Move execution:
  - `make_move()` handles normal, promotion, en passant, castling, castling-right updates, EP updates, clocks, side switch, and hash changes
- Null move support for search pruning:
  - `make_null_move()`

### `attacks.h` / `attacks.cpp`

Attack generation backend.

Includes:

- Precomputed lookup tables:
  - Pawn attacks by color/square
  - Knight attacks
  - King attacks
- Sliding attack support:
  - Magic metadata structs/tables are initialized
  - Runtime attack functions currently use `sliding_attack(...)` for bishops/rooks (and queens via composition in headers)
- Initialization entrypoint:
  - `attacks::init()`

This module powers move generation and attack checks.

### `movegen.h` / `movegen.cpp`

Generates pseudo-legal and legal moves, and exposes terminal/draw helpers.

Core functions:

- Pawn move generation (`generate_pawn_moves`):
  - single/double pushes
  - captures
  - promotions (all four pieces)
  - en passant
- Piece move generation (`generate_piece_moves`):
  - knight/bishop/rook/queen/king moves
  - castling with occupancy and attack checks
- Legal filtering:
  - `generate_legal()` makes pseudo moves, applies each on a copy, keeps only moves that do not leave own king attacked
- State checks:
  - `in_check()`
  - `is_checkmate()`
  - `is_stalemate()`
  - `is_draw_by_fifty_move_rule()`
  - `game_termination()`
- Debug/perf:
  - `perft()`

### `zobrist.h` / `zobrist.cpp`

Zobrist hashing key tables and initialization:

- `pieceKeys[12][64]`
- `castlingKeys[16]`
- `enPassantKeys[8]`
- `sideKey`
- `zobrist::init()` fills tables using deterministic xorshift PRNG seed.

`Board` relies on this for incremental hash maintenance and TT addressing.

### `eval.h` / `eval.cpp`

Static evaluation model in centipawns.

Evaluation terms:

- Material via `PieceValue`
- Piece-square tables (PST) for each piece type
- Black PST lookup uses mirrored square

Main API:

- `int evaluate(const Board&)`

Return convention:

- Score is from side-to-move perspective (positive means better for player to move).

### `tt.h` / `tt.cpp`

Transposition table implementation.

Data:

- `TTEntry`: hash key, score, depth, bound flag, best move
- Flags:
  - `TT_EXACT`
  - `TT_ALPHA` (upper bound)
  - `TT_BETA` (lower bound)

Behavior:

- Table size selected from MB input, rounded to power-of-two entry count
- Indexing by `key & mask`
- Replacement policy: always replace
- API:
  - `store(...)`
  - `probe(...)`
  - `clear()`

### `search.h` / `search.cpp`

Search state and all search logic.

Public APIs:

- `search(const Board&, int timeLimitMs, TranspositionTable&)`
- `searchDepth(const Board&, int depth, TranspositionTable&)`

Shared constants and state:

- `MATE_SCORE`, `MAX_PLY`
- `SearchState` stores:
  - TT reference
  - killer moves table
  - history heuristic table
  - timer and stop flag

## Search Algorithm Breakdown

This section explains how the engine finds the best move in the *actual runtime order the code executes*, including the conditions that enable/disable each pruning/reduction.

At a high level, the call chain is:

`search(...)` (iterative deepening, aspiration) → `searchRoot(...)` (root move loop) → `negamax(...)` (alpha-beta + pruning) → `quiescence(...)` (capture-only leaf extension) → `evaluate(...)`

### 1. Top-Level Driver: Iterative Deepening + Aspiration Windows (`search`)

Entry point: `SearchResult search(const Board&, int timeLimitMs, TranspositionTable&)`

Sequential flow:

1. Create `SearchState` and start the wall-clock timer (`SearchState::checkTime()` sets `stopped`).
2. For `depth = 1..MAX_PLY`, run one full root iteration (`searchRoot`).
3. Aspiration windows apply only after the first completed iteration:
   - If `depth <= 1`, use full window `[-MATE_SCORE-1, MATE_SCORE+1]`.
   - If `depth >= 2`, set `alpha = lastScore - ASPIRATION_WINDOW`, `beta = lastScore + ASPIRATION_WINDOW`.
   - Re-run `searchRoot` while the result fails low/high:
     - Fail-low (`score <= alpha`): decrease `alpha`, double `delta`, retry.
     - Fail-high (`score >= beta`): increase `beta`, double `delta`, retry.
4. Stop handling:
   - If the time limit triggers inside an iteration, `SearchState.stopped` causes the engine to keep the previous completed iteration’s `bestResult`.
5. Early mate exit:
   - If a score is in the mate band, break out early.

### 2. Root Iteration: Move Loop and “Best Move” Selection (`searchRoot`)

Entry point: `static SearchResult searchRoot(const Board&, int depth, int alpha, int beta, SearchState&)`

Sequential flow:

1. Generate legal root moves via `generate_legal(board)`.
2. Probe the TT once for root ordering:
   - If a TT entry exists, its `bestMove` is used as the first ordering hint.
3. Score all root moves and repeatedly select the next best-scored move:
   - Scoring is done by `scoreMoves(...)`.
   - Selection uses `pickBest(...)` (swap the best remaining move into position `i`).
4. For each root move (in ordered sequence):
   - Make the move on a copy and call the main search: `score = -negamax(child, depth - 1, -beta, -alpha, ...)`.
   - Track the maximum `bestScore` and corresponding `bestMove`.
   - Raise root `alpha` when a move improves it (`score > alpha`).
5. If the search wasn’t stopped, store the root result in the TT.

The move you get back from `search()` / `searchDepth()` is simply the argmax over the root move loop above.

### 3. Main Search Node: Alpha-Beta Negamax with Pruning (`negamax`)

Entry point: `static int negamax(const Board&, int depth, int alpha, int beta, SearchState&, int ply, bool allowNullMove)`

This is the “inner loop” that does most of the work. The techniques are applied in this order:

#### 3.1 Hard Stops (time / external stop)

1. If `state.stopped` is already true, return immediately.
2. Call `state.checkTime()` and stop if the time limit has been exceeded.

#### 3.2 Transposition Table Probe (TT cutoffs + ordering hint)

1. Probe `state.tt` using `board.hash_key()`.
2. If the stored depth is deep enough and the bound is compatible with the current window:
   - `TT_EXACT`: return stored score
   - `TT_BETA` and stored score >= `beta`: return stored score (fail-high)
   - `TT_ALPHA` and stored score <= `alpha`: return stored score (fail-low)
3. Regardless of cutoff, keep `ttEntry.bestMove` as a move-ordering hint for this node.

#### 3.3 Leaf Handling: Quiescence at Depth 0

If `depth == 0`, the node is evaluated by calling `quiescence(board, alpha, beta, state)`.

Quiescence exists to avoid “noisy” horizon artifacts by extending tactical capture sequences until a stable position is reached.

#### 3.4 Legal Move Generation + Terminal/Draw Detection

1. Generate legal moves (`generate_legal(board)`).
2. If no legal moves:
   - If `in_check(board)` is true: checkmate score `-MATE_SCORE + ply`
   - Otherwise: stalemate score `0`
3. If `is_draw_by_fifty_move_rule(board)` is true: return `0`.

#### 3.5 Compute Node Context Flags (controls pruning)

These values gate several heuristics:

- `inCheck = in_check(board)`:
  - Many pruning steps are disabled in check.
- `pvNode = (beta - alpha > 1)`:
  - Used as a “principal variation” indicator: pruning is typically disabled in PV nodes to preserve exactness.
- `staticEval = evaluate(board)`:
  - Used for static (non-search) pruning tests like futility and reverse futility.

#### 3.6 Reverse Futility Pruning (RFP)

Applied *before* null move and before the move loop.

Enabled only if all of the following are true:

- Not a PV node (`!pvNode`)
- Not in check (`!inCheck`)
- Shallow enough (`depth <= FUTILITY_MAX_DEPTH`)
- Not in mate-score range (`abs(beta) < MATE_SCORE - MAX_PLY`)

Prune condition:

- If `staticEval - RFP_MARGIN[depth] >= beta`, immediately return `staticEval - RFP_MARGIN[depth]`.

Intuition:

- If static eval already exceeds beta by a safety margin, searching moves is unlikely to change the fail-high outcome.

#### 3.7 Null Move Pruning (NMP) + Verified NMP

Applied after RFP and before the normal move loop.

Enabled only if all of the following are true:

- Null moves allowed in this subtree (`allowNullMove == true`)
- Not in check (`!inCheck`)
- Depth is large enough (`depth >= NMP_MIN_DEPTH`)
- Side to move has enough non-pawn material (`nonPawnMaterial(...) >= NMP_MIN_MATERIAL`)

Sequential flow:

1. Create `nullChild = board`, call `nullChild.make_null_move()`.
2. Search at reduced depth with a null window:
   - `nullScore = -negamax(nullChild, nullDepth, -beta, -beta + 1, ..., allowNullMove=false)`
3. If `nullScore >= beta`, it is a candidate fail-high cutoff:
   - If `depth >= NMP_VERIFY_DEPTH`, re-search with null moves disabled to verify zugzwang-sensitive cases.
   - Otherwise, accept the cutoff and return `beta`.

#### 3.8 Normal Move Loop (ordering → optional pruning/reductions → recursive search)

1. Score all legal moves with `scoreMoves(...)` using:
   - TT move first, captures (MVV-LVA), killers, history.
2. For `i = 0..moves.size()-1`:
   - Select the next best move via `pickBest(moves, scores, i)`.
   - Classify the move:
     - `capture = isCapture(board, m)`
     - `isPromotion = (move_type(m) == Promotion)`

##### Futility pruning (skip weak quiet moves near the leaves)

This happens *before* making the move, and only applies to “quiet-ish” moves.

Enabled only if all of the following are true:

- Not a PV node (`!pvNode`)
- Not in check (`!inCheck`)
- Shallow enough (`depth <= FUTILITY_MAX_DEPTH`)
- Not the first move in the ordered list (`i > 0`)
- Not a capture and not a promotion (`!capture && !isPromotion`)
- Not in mate-score range (`abs(alpha) < MATE_SCORE - MAX_PLY`)

Prune condition:

- If `staticEval + FUTILITY_MARGIN[depth] <= alpha`, skip this move (`continue`).

##### Make the move and search it

1. `child = board`, then `child.make_move(m)`.
2. Choose between full-depth search and LMR:

###### LMR (late move reductions) for late quiet moves

Enabled only if all of the following are true:

- Not in check (`!inCheck`)
- Depth is large enough (`depth >= LMR_MIN_DEPTH`)
- Move is “late” (`i >= LMR_FULL_SEARCH_MOVES`)
- Move is quiet and non-promotion (`!capture && !isPromotion`)

Sequential flow:

1. Reduced-depth zero-window search: `[-alpha-1, -alpha]`
2. If it beats alpha, re-search at full depth with zero-window.
3. If it still looks promising (between alpha and beta), do a full-window re-search `[-beta, -alpha]`.

###### Full search (no LMR)

If LMR is not enabled, do the regular full-window recursive call:

- `score = -negamax(child, depth - 1, -beta, -alpha, ...)`

##### Alpha-beta cutoffs + heuristic updates

1. If `score >= beta`:
   - Store a TT lower bound (`TT_BETA`) for this node.
   - If the cutoff move is quiet, update:
     - killer moves for this ply
     - history table for this side/from/to
   - Return `beta` (fail-high cutoff).
2. If `score > alpha`:
   - Raise `alpha`
   - Track `bestMove`
   - Mark the node’s TT flag as `TT_EXACT` (the node produced a new best score).

3. After the loop finishes:
   - Store the node result in the TT as either:
     - `TT_EXACT` if alpha improved during the loop
     - `TT_ALPHA` otherwise (no move raised alpha; the node is an upper bound)
   - Return `alpha`.

### 4. Quiescence Search: Stabilizing Leaves (`quiescence`)

Entry point: `static int quiescence(const Board&, int alpha, int beta, SearchState&)`

Sequential flow:

1. Compute `standPat = evaluate(board)`.
2. Apply stand-pat alpha-beta:
   - If `standPat >= beta`, return `beta`.
   - If `standPat > alpha`, set `alpha = standPat`.
3. Generate legal moves, filter to captures only.
4. Order captures by MVV-LVA (`scoreCapturesMvvLva` + `pickBest`).
5. For each capture:
   - Delta pruning gate:
     - If `standPat + captureValue(m) + DELTA_MARGIN < alpha`, skip the capture.
   - Otherwise recurse: `score = -quiescence(child, -beta, -alpha, ...)`.
   - Apply alpha-beta cutoffs and raise `alpha` if improved.
6. Return `alpha` when no captures can improve it.

## Tests and Build

### `tests/test_board.cpp`

Board/FEN/hash/bitboard correctness checks.

### `tests/test_movegen.cpp`

Attack generation, move making, perft, and termination checks.

### `tests/test_search.cpp`

TT behavior, eval sanity, tactical search (mate/stalemate), and iterative deepening behavior.

### `tests/CMakeLists.txt` and `CMakeLists.txt`

Builds the engine library and test executables via CMake.

## Practical Starting Points

If you are extending the engine:

1. New search heuristic: start in `search.cpp`.
2. New evaluation term: add in `eval.cpp`.
3. Move legality/variant rules: adjust `movegen.cpp` and potentially `board.cpp`.
4. Hash-dependent features: verify incremental hash in `board.cpp` and table usage in `tt.cpp`.

## Summary 
Null move → prune hopeless positions

Reverse futility → prune clearly winning nodes

LMR → reduce unlikely moves

Futility → prune weak leaf moves

Delta pruning → prune hopeless captures

Quiescence → stabilize leaves

```
Iterative Deepening
  → Aspiration Windows
    → Alpha-Beta
        → Reverse Futility Pruning
        → Null Move
        → Move Loop
            → Late Move Reductions
        → Depth == 0
            → Futility Pruning
            → Quiescence
```


## TODO

1. Principle Variation Search
2. Static Exchange Evaluation
3. Better heuristics 
