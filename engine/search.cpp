#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace panda {

// ============================================================
// Move ordering
// ============================================================

static constexpr int TT_MOVE_SCORE    = 10000000;
static constexpr int CAPTURE_BASE     =  1000000;
static constexpr int KILLER1_SCORE    =   900000;
static constexpr int KILLER2_SCORE    =   800000;

// Delta pruning margin: a small safety buffer beyond the captured piece value
static constexpr int DELTA_MARGIN     = 200;

// Aspiration window initial half-width
static constexpr int ASPIRATION_WINDOW = 50;

// Late move reduction parameters
static constexpr int LMR_MIN_DEPTH     = 3;   // Only apply LMR at depth >= 3
static constexpr int LMR_FULL_SEARCH_MOVES = 3; // Search first N moves at full depth

// Futility pruning margins indexed by depth (depth 1..3)
static constexpr int FUTILITY_MARGIN[4] = { 0, 200, 350, 500 };
// Reverse futility pruning margins indexed by depth (depth 1..3)
static constexpr int RFP_MARGIN[4] = { 0, 100, 250, 400 };
static constexpr int FUTILITY_MAX_DEPTH = 3;

// Null move pruning parameters
static constexpr int NMP_MIN_DEPTH     = 3;   // Only apply NMP at depth >= 3
static constexpr int NMP_REDUCTION     = 2;   // Standard reduction
static constexpr int NMP_VERIFY_DEPTH  = 6;   // Use verified NMP at depth >= 6
static constexpr int NMP_MIN_MATERIAL  = 400;  // Minimum non-pawn material to allow NMP

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

static void scoreMoves(const Board& board, MoveList& moves, int* scores,
                       Move ttMove, const SearchState& state, int ply) {
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

static int quiescence(const Board& board, int alpha, int beta, SearchState& state) {
    if (state.stopped) return 0;

    int standPat = evaluate(board);

    if (standPat >= beta)
        return beta;
    if (standPat > alpha)
        alpha = standPat;

    MoveList allMoves = generate_legal(board);

    // Filter to captures only
    MoveList captures;
    for (int i = 0; i < allMoves.size(); ++i) {
        if (isCapture(board, allMoves[i]))
            captures.add(allMoves[i]);
    }

    // MVV-LVA ordering for captures
    int scores[256];
    scoreCapturesMvvLva(board, captures, scores);

    for (int i = 0; i < captures.size(); ++i) {
        pickBest(captures, scores, i);
        Move m = captures[i];

        // Delta pruning: skip if the capture + margin can't possibly raise alpha
        if (standPat + captureValue(board, m) + DELTA_MARGIN < alpha)
            continue;

        Board child = board;
        child.make_move(m);

        int score = -quiescence(child, -beta, -alpha, state);

        if (state.stopped) return 0;

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

static int lmrTable[64][64]; // [depth][moveIndex]

static bool lmrInitialized = []() {
    for (int d = 0; d < 64; ++d)
        for (int m = 0; m < 64; ++m)
            lmrTable[d][m] = (d > 0 && m > 0)
                ? static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25)
                : 0;
    return true;
}();

// ============================================================
// Null move pruning helpers
// ============================================================

// Non-pawn material for one side (knights, bishops, rooks, queens)
static int nonPawnMaterial(const Board& board, Color c) {
    return popcount(board.pieces(c, Knight)) * PieceValue[Knight]
         + popcount(board.pieces(c, Bishop)) * PieceValue[Bishop]
         + popcount(board.pieces(c, Rook))   * PieceValue[Rook]
         + popcount(board.pieces(c, Queen))  * PieceValue[Queen];
}

// ============================================================
// Negamax with alpha-beta pruning
// ============================================================

static int negamax(const Board& board, int depth, int alpha, int beta,
                   SearchState& state, int ply, bool allowNullMove = true) {
    if (state.stopped) return 0;

    // Periodically check time (every node is fine for now)
    if (state.checkTime()) return 0;

    // TT probe
    TTEntry ttEntry;
    Move ttMove = NullMove;
    if (state.tt.probe(board.hash_key(), ttEntry)) {
        ttMove = ttEntry.bestMove;
        if (ttEntry.depth >= depth) {
            if (ttEntry.flag == TT_EXACT)
                return ttEntry.score;
            if (ttEntry.flag == TT_BETA && ttEntry.score >= beta)
                return ttEntry.score;
            if (ttEntry.flag == TT_ALPHA && ttEntry.score <= alpha)
                return ttEntry.score;
        }
    }

    MoveList moves = generate_legal(board);

    // Terminal node detection
    if (moves.size() == 0) {
        if (in_check(board))
            return -MATE_SCORE + ply; // Checkmate
        return 0;                     // Stalemate
    }

    // Base case: quiescence search
    if (depth == 0)
        return quiescence(board, alpha, beta, state);

    // 50-move rule draw
    if (is_draw_by_fifty_move_rule(board))
        return 0;

    bool inCheck = in_check(board);
    bool pvNode = (beta - alpha > 1);
    int staticEval = evaluate(board);

    // Reverse futility pruning (static null move pruning)
    // If our position is so good that even after a margin we still beat beta, prune.
    // Skip in PV nodes to preserve exact scores on the principal variation.
    if (!pvNode
        && !inCheck
        && depth <= FUTILITY_MAX_DEPTH
        && std::abs(beta) < MATE_SCORE - MAX_PLY
        && staticEval - RFP_MARGIN[depth] >= beta) {
        return staticEval - RFP_MARGIN[depth];
    }

    // Null move pruning
    if (allowNullMove
        && !inCheck
        && depth >= NMP_MIN_DEPTH
        && nonPawnMaterial(board, board.side_to_move()) >= NMP_MIN_MATERIAL) {

        Board nullChild = board;
        nullChild.make_null_move();

        int reduction = NMP_REDUCTION + (depth > 6 ? 1 : 0); // deeper nodes get extra reduction
        int nullDepth = depth - 1 - reduction;
        if (nullDepth < 0) nullDepth = 0;

        int nullScore = -negamax(nullChild, nullDepth, -beta, -beta + 1, state, ply + 1, false);

        if (state.stopped) return 0;

        if (nullScore >= beta) {
            // Verified null move pruning at deeper nodes
            if (depth >= NMP_VERIFY_DEPTH) {
                // Re-search at reduced depth with null moves disabled to verify
                int verifyScore = negamax(board, nullDepth, beta - 1, beta, state, ply, false);
                if (state.stopped) return 0;
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
        if (!pvNode
            && !inCheck
            && depth <= FUTILITY_MAX_DEPTH
            && i > 0  // never prune the first move (ensures we have a legal move)
            && !capture
            && !isPromotion
            && std::abs(alpha) < MATE_SCORE - MAX_PLY
            && staticEval + FUTILITY_MARGIN[depth] <= alpha) {
            continue;
        }

        Board child = board;
        child.make_move(m);

        int score;

        // Late Move Reductions
        bool doLMR = !inCheck
            && depth >= LMR_MIN_DEPTH
            && i >= LMR_FULL_SEARCH_MOVES
            && !capture
            && !isPromotion;

        if (doLMR) {
            int d = depth - 1;
            int mi = (i < 64) ? i : 63;
            int reduction = lmrTable[(d < 64) ? d : 63][mi];
            if (reduction < 1) reduction = 1;
            int reducedDepth = depth - 1 - reduction;
            if (reducedDepth < 0) reducedDepth = 0;

            // Reduced-depth zero-window search
            score = -negamax(child, reducedDepth, -alpha - 1, -alpha, state, ply + 1);

            // Re-search at full depth with zero window if it beats alpha
            if (!state.stopped && score > alpha) {
                score = -negamax(child, depth - 1, -alpha - 1, -alpha, state, ply + 1);
            }

            // Full window re-search if it still beats alpha (PVS-style)
            if (!state.stopped && score > alpha && score < beta) {
                score = -negamax(child, depth - 1, -beta, -alpha, state, ply + 1);
            }
        } else {
            score = -negamax(child, depth - 1, -beta, -alpha, state, ply + 1);
        }

        if (state.stopped) return 0;

        if (score >= beta) {
            state.tt.store(board.hash_key(), score, depth, TT_BETA, m);

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

    state.tt.store(board.hash_key(), alpha, depth, flag, bestMove);
    return alpha;
}

// ============================================================
// Root search (single depth iteration)
// ============================================================

static SearchResult searchRoot(const Board& board, int depth, int alpha, int beta,
                               SearchState& state) {
    MoveList moves = generate_legal(board);

    if (moves.size() == 0)
        return {NullMove, 0};

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

        int score = -negamax(child, depth - 1, -beta, -alpha, state, 1);

        if (state.stopped) break;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha)
            alpha = score;
    }

    if (!state.stopped)
        state.tt.store(board.hash_key(), bestScore, depth, TT_EXACT, bestMove);

    return {bestMove, bestScore};
}

// ============================================================
// Public API
// ============================================================

SearchResult search(const Board& board, int timeLimitMs, TranspositionTable& tt) {
    SearchState state(tt);
    state.startTime = std::chrono::steady_clock::now();
    state.timeLimitMs = timeLimitMs;

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
            int beta  = bestResult.score + delta;

            while (true) {
                result = searchRoot(board, depth, alpha, beta, state);
                if (state.stopped) break;

                if (result.score <= alpha) {
                    // Fail low: widen alpha
                    alpha = (alpha - delta > -MATE_SCORE - 1) ? alpha - delta : -MATE_SCORE - 1;
                    delta *= 2;
                } else if (result.score >= beta) {
                    // Fail high: widen beta
                    beta = (beta + delta < MATE_SCORE + 1) ? beta + delta : MATE_SCORE + 1;
                    delta *= 2;
                } else {
                    break; // Score within window
                }
            }
        }

        if (state.stopped)
            break; // Discard partial iteration, use previous result
        bestResult = result;

        // Early exit if we found a forced mate
        if (bestResult.score > MATE_SCORE - MAX_PLY ||
            bestResult.score < -MATE_SCORE + MAX_PLY)
            break;
    }

    return bestResult;
}

SearchResult searchDepth(const Board& board, int depth, TranspositionTable& tt) {
    SearchState state(tt);
    state.startTime = std::chrono::steady_clock::now();
    state.timeLimitMs = 0; // 0 means no time limit

    return searchRoot(board, depth, -MATE_SCORE - 1, MATE_SCORE + 1, state);
}

} // namespace panda
