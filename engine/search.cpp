#include "search.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>

#include "eval.h"
#include "movegen.h"

namespace panda {

struct SearchState {
    TranspositionTable& tt;
    Move killers[MAX_PLY][2];  // 2 killer moves per ply
    int history[2][64][64];    // [color][from][to] history scores
    std::vector<uint64_t> repetitionHistory;
    int rootRepIndex;
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs;
    bool stopped;
    std::atomic<bool>* externalStop;  // set by UCI "stop" command
    uint64_t nodes;

    explicit SearchState(TranspositionTable& tt_, std::atomic<bool>* extStop = nullptr)
        : tt(tt_),
          rootRepIndex(0),
          timeLimitMs(0),
          stopped(false),
          externalStop(extStop),
          nodes(0) {
        clear();
    }

    void clear() {
        std::memset(killers, 0, sizeof(killers));
        std::memset(history, 0, sizeof(history));
        stopped = false;
        nodes = 0;
        repetitionHistory.clear();
        rootRepIndex = 0;
    }

    bool checkTime() {
        // Check external stop flag (from UCI "stop")
        if (externalStop && externalStop->load(std::memory_order_relaxed)) {
            stopped = true;
            return true;
        }
        if (timeLimitMs <= 0)
            return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= timeLimitMs) {
            stopped = true;
            return true;
        }
        return false;
    }
};

static void initRepetitionHistory(SearchState& state, const Board& board,
                                  const std::vector<uint64_t>& history) {
    state.repetitionHistory = history;
    if (state.repetitionHistory.empty() || state.repetitionHistory.back() != board.hash_key()) {
        state.repetitionHistory.push_back(board.hash_key());
    }
    state.rootRepIndex = static_cast<int>(state.repetitionHistory.size()) - 1;
}

static bool isThreefoldRepetition(const Board& board, const SearchState& state, int repIndex) {
    if (repIndex < 0 || repIndex >= static_cast<int>(state.repetitionHistory.size()))
        return false;
    if (board.halfmove_clock() < 4)
        return false;

    uint64_t key = board.hash_key();
    int count = 1;
    int maxBack = std::min(board.halfmove_clock(), repIndex);

    // Same-side positions occur every 2 plies.
    for (int i = repIndex - 2; i >= 0 && (repIndex - i) <= maxBack; i -= 2) {
        if (state.repetitionHistory[i] == key) {
            ++count;
            if (count >= 3)
                return true;
        }
    }
    return false;
}

// ============================================================
// Move ordering
// ============================================================

static constexpr int TT_MOVE_SCORE = 10000000;
static constexpr int CAPTURE_BASE = 1000000;
static constexpr int KILLER1_SCORE = 900000;
static constexpr int KILLER2_SCORE = 800000;

// Delta pruning margin: a small safety buffer beyond the captured piece value
static constexpr int DELTA_MARGIN = 200;

// Aspiration window initial half-width
static constexpr int ASPIRATION_WINDOW = 50;

// Late move reduction parameters
static constexpr int LMR_MIN_DEPTH = 3;          // Only apply LMR at depth >= 3
static constexpr int LMR_FULL_SEARCH_MOVES = 3;  // Search first N moves at full depth

// Futility pruning margins indexed by depth (depth 1..3)
static constexpr int FUTILITY_MARGIN[4] = {0, 200, 350, 500};
// Reverse futility pruning margins indexed by depth (depth 1..3)
static constexpr int RFP_MARGIN[4] = {0, 100, 250, 400};
static constexpr int FUTILITY_MAX_DEPTH = 3;

// Null move pruning parameters
static constexpr int NMP_MIN_DEPTH = 3;       // Only apply NMP at depth >= 3
static constexpr int NMP_REDUCTION = 2;       // Standard reduction
static constexpr int NMP_VERIFY_DEPTH = 6;    // Use verified NMP at depth >= 6
static constexpr int NMP_MIN_MATERIAL = 400;  // Minimum non-pawn material to allow NMP

static bool isCapture(const Board& board, Move m) {
    return board.piece_on(move_to(m)) != NoPiece || move_type(m) == EnPassant;
}

static int mvvLvaScore(const Board& board, Move m) {
    Square to = move_to(m);
    Square from = move_from(m);

    int victimVal;
    if (move_type(m) == EnPassant) {
        victimVal = PieceValue[Pawn];
    } else {
        Piece victim = board.piece_on(to);
        victimVal = PieceValue[piece_type(victim)];
    }

    Piece attacker = board.piece_on(from);
    int attackerVal = PieceValue[piece_type(attacker)];

    return CAPTURE_BASE + victimVal * 10 - attackerVal;
}

static void scoreMoves(const Board& board, MoveList& moves, int* scores, Move ttMove,
                       const SearchState& state, int ply) {
    Color side = board.side_to_move();
    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];

        if (m == ttMove && ttMove != NullMove) {
            scores[i] = TT_MOVE_SCORE;
        } else if (isCapture(board, m)) {
            scores[i] = mvvLvaScore(board, m);
        } else if (ply < MAX_PLY && m == state.killers[ply][0]) {
            scores[i] = KILLER1_SCORE;
        } else if (ply < MAX_PLY && m == state.killers[ply][1]) {
            scores[i] = KILLER2_SCORE;
        } else {
            // History heuristic
            scores[i] = state.history[side][move_from(m)][move_to(m)];
        }
    }
}

// Incremental selection: pick the best-scored move at position idx and swap it in
static void pickBest(MoveList& moves, int* scores, int idx) {
    int bestIdx = idx;
    int bestScore = scores[idx];
    for (int i = idx + 1; i < moves.size(); ++i) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            bestIdx = i;
        }
    }
    if (bestIdx != idx) {
        std::swap(moves.moves[idx], moves.moves[bestIdx]);
        std::swap(scores[idx], scores[bestIdx]);
    }
}

// Score captures only (for quiescence search)
static void scoreCapturesMvvLva(const Board& board, MoveList& moves, int* scores) {
    for (int i = 0; i < moves.size(); ++i) {
        scores[i] = mvvLvaScore(board, moves[i]);
    }
}

// ============================================================
// Quiescence search
// ============================================================

static int captureValue(const Board& board, Move m) {
    int value = 0;

    // Add captured piece value
    if (move_type(m) == EnPassant) {
        value = PieceValue[Pawn];
    } else {
        Piece victim = board.piece_on(move_to(m));
        if (victim != NoPiece)
            value = PieceValue[piece_type(victim)];
    }

    // Add promotion gain (Queen value minus Pawn value)
    if (move_type(m) == Promotion) {
        PieceType promoPt = promotion_type(m);
        value += PieceValue[promoPt] - PieceValue[Pawn];
    }

    return value;
}

static int quiescence(const Board& board, int alpha, int beta, SearchState& state, int ply,
                      int repIndex) {
    if (state.stopped)
        return 0;
    if (state.checkTime())
        return 0;
    ++state.nodes;

    if (isThreefoldRepetition(board, state, repIndex))
        return 0;

    bool inCheck = in_check(board);
    MoveList allMoves = generate_legal(board);
    MoveList qmoves;

    if (inCheck) {
        qmoves = allMoves;
    } else {
        // Stand pat only if not in check
        int standPat = evaluate(board);

        if (standPat >= beta)
            return beta;
        if (standPat > alpha)
            alpha = standPat;

        for (int i = 0; i < allMoves.size(); ++i) {
            if (isCapture(board, allMoves[i]))
                qmoves.add(allMoves[i]);
        }
    }

    // Check for checkmate/stalemate
    if (qmoves.size() == 0) {
        if (inCheck)
            return -MATE_SCORE + ply;  // Checkmate: lose in 'ply' half-moves
        return alpha;                  // Stalemate
    }

    // MVV-LVA ordering for captures
    int scores[256];
    scoreCapturesMvvLva(board, qmoves, scores);

    for (int i = 0; i < qmoves.size(); ++i) {
        pickBest(qmoves, scores, i);
        Move m = qmoves[i];

        // Delta pruning: skip if the capture + margin can't possibly raise alpha
        // Only apply when not in check and we have a valid standPat
        if (!inCheck) {
            int standPat = evaluate(board);
            if (standPat + captureValue(board, m) + DELTA_MARGIN < alpha)
                continue;
        }

        Board child = board;
        child.make_move(m);
        int childRepIndex = repIndex + 1;
        if (childRepIndex >= static_cast<int>(state.repetitionHistory.size()))
            state.repetitionHistory.push_back(child.hash_key());
        else
            state.repetitionHistory[childRepIndex] = child.hash_key();

        int score = -quiescence(child, -beta, -alpha, state, ply + 1, childRepIndex);

        if (state.stopped)
            return 0;

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

// ============================================================
// LMR reduction table (initialized once)
// ============================================================

static int lmrTable[64][64];  // [depth][moveIndex]

static bool lmrInitialized = []() {
    for (int d = 0; d < 64; ++d)
        for (int m = 0; m < 64; ++m)
            lmrTable[d][m] =
                (d > 0 && m > 0) ? static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25) : 0;
    return true;
}();

// ============================================================
// Null move pruning helpers
// ============================================================

// Non-pawn material for one side (knights, bishops, rooks, queens)
static int nonPawnMaterial(const Board& board, Color c) {
    return popcount(board.pieces(c, Knight)) * PieceValue[Knight] +
           popcount(board.pieces(c, Bishop)) * PieceValue[Bishop] +
           popcount(board.pieces(c, Rook)) * PieceValue[Rook] +
           popcount(board.pieces(c, Queen)) * PieceValue[Queen];
}

// ============================================================
// Mate score normalization helpers
// ============================================================

static int scoreToTT(int score, int ply) {
    if (score >= MATE_SCORE - MAX_PLY)
        return score + ply;  // Mate for us: increase distance
    if (score <= -MATE_SCORE + MAX_PLY)
        return score - ply;  // Mate against us: decrease (more negative)
    return score;
}

static int scoreFromTT(int score, int ply) {
    if (score >= MATE_SCORE - MAX_PLY)
        return score - ply;  // Mate for us: decrease distance to current ply
    if (score <= -MATE_SCORE + MAX_PLY)
        return score + ply;  // Mate against us: increase (less negative)
    return score;
}

// ============================================================
// Negamax with alpha-beta pruning
// ============================================================

static int negamax(const Board& board, int depth, int alpha, int beta, SearchState& state, int ply,
                   int repIndex, bool allowNullMove = true) {
    if (state.stopped)
        return 0;

    // Periodically check time (every node is fine for now)
    if (state.checkTime())
        return 0;

    ++state.nodes;

    if (isThreefoldRepetition(board, state, repIndex))
        return 0;

    MoveList moves = generate_legal(board);

    // Terminal node detection
    if (moves.size() == 0) {
        if (in_check(board))
            return -MATE_SCORE + ply;  // Checkmate
        return 0;                      // Stalemate
    }

    // 50-move rule draw
    if (is_draw_by_fifty_move_rule(board))
        return 0;

    // TT probe
    TTEntry ttEntry;
    Move ttMove = NullMove;
    if (state.tt.probe(board.hash_key(), ttEntry)) {
        ttMove = ttEntry.bestMove;
        if (ttEntry.depth >= depth) {
            int ttScore = scoreFromTT(ttEntry.score, ply);
            if (ttEntry.flag == TT_EXACT)
                return ttScore;
            if (ttEntry.flag == TT_BETA && ttScore >= beta)
                return ttScore;
            if (ttEntry.flag == TT_ALPHA && ttScore <= alpha)
                return ttScore;
        }
    }

    // Base case: quiescence search
    if (depth == 0)
        return quiescence(board, alpha, beta, state, ply, repIndex);

    bool inCheck = in_check(board);
    bool pvNode = (beta - alpha > 1);
    int staticEval = evaluate(board);

    // Reverse futility pruning (static null move pruning)
    // If our position is so good that even after a margin we still beat beta, prune.
    // Skip in PV nodes to preserve exact scores on the principal variation.
    if (!pvNode && !inCheck && depth <= FUTILITY_MAX_DEPTH &&
        std::abs(beta) < MATE_SCORE - MAX_PLY && staticEval - RFP_MARGIN[depth] >= beta) {
        return staticEval - RFP_MARGIN[depth];
    }

    // Null move pruning
    if (allowNullMove && !inCheck && depth >= NMP_MIN_DEPTH &&
        nonPawnMaterial(board, board.side_to_move()) >= NMP_MIN_MATERIAL) {
        Board nullChild = board;
        nullChild.make_null_move();

        int reduction = NMP_REDUCTION + (depth > 6 ? 1 : 0);  // deeper nodes get extra reduction
        int nullDepth = depth - 1 - reduction;
        if (nullDepth < 0)
            nullDepth = 0;
        int nullRepIndex = repIndex + 1;
        if (nullRepIndex >= static_cast<int>(state.repetitionHistory.size()))
            state.repetitionHistory.push_back(nullChild.hash_key());
        else
            state.repetitionHistory[nullRepIndex] = nullChild.hash_key();

        int nullScore =
            -negamax(nullChild, nullDepth, -beta, -beta + 1, state, ply + 1, nullRepIndex, false);

        if (state.stopped)
            return 0;

        if (nullScore >= beta) {
            // Verified null move pruning at deeper nodes
            if (depth >= NMP_VERIFY_DEPTH) {
                // Re-search at reduced depth with null moves disabled to verify
                int verifyScore =
                    negamax(board, depth - 1, beta - 1, beta, state, ply, repIndex, false);
                if (state.stopped)
                    return 0;
                if (verifyScore >= beta)
                    return beta;
            } else {
                return beta;
            }
        }
    }

    // Score and order moves
    int scores[256];
    scoreMoves(board, moves, scores, ttMove, state, ply);

    Move bestMove = moves[0];
    TTFlag flag = TT_ALPHA;

    for (int i = 0; i < moves.size(); ++i) {
        pickBest(moves, scores, i);
        Move m = moves[i];
        bool capture = isCapture(board, m);
        bool isPromotion = move_type(m) == Promotion;

        // Futility pruning: near leaf, quiet moves that can't possibly raise alpha
        if (!pvNode && !inCheck && depth <= FUTILITY_MAX_DEPTH &&
            i > 0  // never prune the first move (ensures we have a legal move)
            && !capture && !isPromotion && std::abs(alpha) < MATE_SCORE - MAX_PLY &&
            staticEval + FUTILITY_MARGIN[depth] <= alpha) {
            continue;
        }

        Board child = board;
        child.make_move(m);
        int childRepIndex = repIndex + 1;
        if (childRepIndex >= static_cast<int>(state.repetitionHistory.size()))
            state.repetitionHistory.push_back(child.hash_key());
        else
            state.repetitionHistory[childRepIndex] = child.hash_key();

        int score;

        // Late Move Reductions
        bool doLMR = !inCheck && depth >= LMR_MIN_DEPTH && i >= LMR_FULL_SEARCH_MOVES && !capture &&
                     !isPromotion;

        if (doLMR) {
            int d = depth - 1;
            int mi = (i < 64) ? i : 63;
            int reduction = lmrTable[(d < 64) ? d : 63][mi];
            if (reduction < 1)
                reduction = 1;
            int reducedDepth = depth - 1 - reduction;
            if (reducedDepth < 0)
                reducedDepth = 0;

            // Reduced-depth zero-window search
            score = -negamax(child, reducedDepth, -alpha - 1, -alpha, state, ply + 1,
                             childRepIndex);

            // Re-search at full depth with zero window if it beats alpha
            if (!state.stopped && score > alpha) {
                score =
                    -negamax(child, depth - 1, -alpha - 1, -alpha, state, ply + 1, childRepIndex);
            }

            // Full window re-search if it still beats alpha (PVS-style)
            if (!state.stopped && score > alpha && score < beta) {
                score = -negamax(child, depth - 1, -beta, -alpha, state, ply + 1, childRepIndex);
            }
        } else {
            score = -negamax(child, depth - 1, -beta, -alpha, state, ply + 1, childRepIndex);
        }

        if (state.stopped)
            return 0;

        if (score >= beta) {
            state.tt.store(board.hash_key(), scoreToTT(score, ply), depth, TT_BETA, m);

            // Update killer moves and history for quiet moves
            if (!capture) {
                if (ply < MAX_PLY) {
                    state.killers[ply][1] = state.killers[ply][0];
                    state.killers[ply][0] = m;
                }
                state.history[board.side_to_move()][move_from(m)][move_to(m)] += depth * depth;
            }

            return beta;
        }
        if (score > alpha) {
            alpha = score;
            bestMove = m;
            flag = TT_EXACT;
        }
    }

    state.tt.store(board.hash_key(), scoreToTT(alpha, ply), depth, flag, bestMove);
    return alpha;
}

// ============================================================
// Root search (single depth iteration)
// ============================================================

static SearchResult searchRoot(const Board& board, int depth, int alpha, int beta,
                               SearchState& state) {
    const int origAlpha = alpha;
    MoveList moves = generate_legal(board);

    if (moves.size() == 0) {
        if (in_check(board))
            return {NullMove, -MATE_SCORE};  // side to move is checkmated at root (ply 0)
        return {NullMove, 0};                // stalemate
    }

    if (isThreefoldRepetition(board, state, state.rootRepIndex))
        return {moves[0], 0};

    // TT move ordering at root
    TTEntry ttEntry;
    Move ttMove = NullMove;
    if (state.tt.probe(board.hash_key(), ttEntry))
        ttMove = ttEntry.bestMove;

    int scores[256];
    scoreMoves(board, moves, scores, ttMove, state, 0);

    Move bestMove = moves[0];
    int bestScore = -MATE_SCORE - 1;

    for (int i = 0; i < moves.size(); ++i) {
        pickBest(moves, scores, i);
        Move m = moves[i];

        Board child = board;
        child.make_move(m);
        int childRepIndex = state.rootRepIndex + 1;
        if (childRepIndex >= static_cast<int>(state.repetitionHistory.size()))
            state.repetitionHistory.push_back(child.hash_key());
        else
            state.repetitionHistory[childRepIndex] = child.hash_key();

        int score = -negamax(child, depth - 1, -beta, -alpha, state, 1, childRepIndex);

        if (state.stopped)
            break;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha)
            alpha = score;

        if (alpha >= beta)
            break;
    }

    if (!state.stopped) {
        // Determine correct TT flag based on window bounds
        TTFlag flag;
        if (bestScore <= origAlpha)
            flag = TT_ALPHA;  // Fail-low: upper bound
        else if (bestScore >= beta)
            flag = TT_BETA;  // Fail-high: lower bound
        else
            flag = TT_EXACT;  // Within window: exact score

        state.tt.store(board.hash_key(), scoreToTT(bestScore, 0), depth, flag, bestMove);
    }

    return {bestMove, bestScore};
}

// ============================================================
// Public API
// ============================================================

SearchResult search(const Board& board, int timeLimitMs, TranspositionTable& tt) {
    SearchState state(tt);
    state.startTime = std::chrono::steady_clock::now();
    state.timeLimitMs = timeLimitMs;
    initRepetitionHistory(state, board, {});

    SearchResult bestResult = {NullMove, 0};

    for (int depth = 1; depth <= MAX_PLY; ++depth) {
        SearchResult result;

        if (depth <= 1) {
            // First iteration: full window
            result = searchRoot(board, depth, -MATE_SCORE - 1, MATE_SCORE + 1, state);
        } else {
            // Aspiration window around previous score
            int delta = ASPIRATION_WINDOW;
            int alpha = bestResult.score - delta;
            int beta = bestResult.score + delta;

            while (true) {
                result = searchRoot(board, depth, alpha, beta, state);
                if (state.stopped)
                    break;

                if (result.score <= alpha) {
                    // Fail low: widen alpha
                    alpha = (alpha - delta > -MATE_SCORE - 1) ? alpha - delta : -MATE_SCORE - 1;
                    delta *= 2;
                } else if (result.score >= beta) {
                    // Fail high: widen beta
                    beta = (beta + delta < MATE_SCORE + 1) ? beta + delta : MATE_SCORE + 1;
                    delta *= 2;
                } else {
                    break;  // Score within window
                }
            }
        }

        if (state.stopped)
            break;  // Discard partial iteration, use previous result
        bestResult = result;

        // Early exit if we found a forced mate
        if (bestResult.score > MATE_SCORE - MAX_PLY || bestResult.score < -MATE_SCORE + MAX_PLY)
            break;
    }

    return bestResult;
}

SearchResult searchDepth(const Board& board, int depth, TranspositionTable& tt) {
    SearchState state(tt);
    state.startTime = std::chrono::steady_clock::now();
    state.timeLimitMs = 0;  // 0 means no time limit
    initRepetitionHistory(state, board, {});

    if (depth < 1)
        depth = 1;

    return searchRoot(board, depth, -MATE_SCORE - 1, MATE_SCORE + 1, state);
}

std::vector<Move> extractPV(const Board& board, TranspositionTable& tt, int maxLen) {
    std::vector<Move> pv;
    Board b = board;
    for (int i = 0; i < maxLen; ++i) {
        TTEntry entry;
        if (!tt.probe(b.hash_key(), entry) || entry.bestMove == NullMove)
            break;
        // Verify the move is legal
        MoveList legal = generate_legal(b);
        bool found = false;
        for (int j = 0; j < legal.size(); ++j) {
            if (legal[j] == entry.bestMove) {
                found = true;
                break;
            }
        }
        if (!found)
            break;
        pv.push_back(entry.bestMove);
        b.make_move(entry.bestMove);
    }
    return pv;
}

SearchResult search(const Board& board, int timeLimitMs, int maxDepth, TranspositionTable& tt,
                    std::atomic<bool>& stopFlag, InfoCallback infoCallback) {
    return search(board, timeLimitMs, maxDepth, tt, stopFlag, {}, infoCallback);
}

SearchResult search(const Board& board, int timeLimitMs, int maxDepth, TranspositionTable& tt,
                    std::atomic<bool>& stopFlag, const std::vector<uint64_t>& repetitionHistory,
                    InfoCallback infoCallback) {
    SearchState state(tt, &stopFlag);
    state.startTime = std::chrono::steady_clock::now();
    state.timeLimitMs = timeLimitMs;
    initRepetitionHistory(state, board, repetitionHistory);

    SearchResult bestResult = {NullMove, 0};

    if (maxDepth < 1)
        maxDepth = MAX_PLY;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        SearchResult result;

        if (depth <= 1) {
            result = searchRoot(board, depth, -MATE_SCORE - 1, MATE_SCORE + 1, state);
        } else {
            int delta = ASPIRATION_WINDOW;
            int alpha = bestResult.score - delta;
            int beta = bestResult.score + delta;

            while (true) {
                result = searchRoot(board, depth, alpha, beta, state);
                if (state.stopped)
                    break;

                if (result.score <= alpha) {
                    alpha = (alpha - delta > -MATE_SCORE - 1) ? alpha - delta : -MATE_SCORE - 1;
                    delta *= 2;
                } else if (result.score >= beta) {
                    beta = (beta + delta < MATE_SCORE + 1) ? beta + delta : MATE_SCORE + 1;
                    delta *= 2;
                } else {
                    break;
                }
            }
        }

        if (state.stopped) {
            // Keep a legal root move even when stopped during depth 1.
            if (depth == 1 && result.bestMove != NullMove)
                bestResult = result;
            break;
        }
        bestResult = result;

        // Send info callback
        if (infoCallback) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - state.startTime).count();

            SearchInfo info;
            info.depth = depth;
            info.score = bestResult.score;
            info.nodes = state.nodes;
            info.timeMs = elapsed;
            info.pv = extractPV(board, tt, depth);

            // Determine if this is a mate score
            if (bestResult.score > MATE_SCORE - MAX_PLY) {
                info.isMate = true;
                info.mateInPly = (MATE_SCORE - bestResult.score + 1) / 2;
            } else if (bestResult.score < -MATE_SCORE + MAX_PLY) {
                info.isMate = true;
                info.mateInPly = -((MATE_SCORE + bestResult.score + 1) / 2);
            } else {
                info.isMate = false;
                info.mateInPly = 0;
            }

            infoCallback(info);
        }

        if (bestResult.score > MATE_SCORE - MAX_PLY || bestResult.score < -MATE_SCORE + MAX_PLY)
            break;
    }

    return bestResult;
}

}  // namespace panda
