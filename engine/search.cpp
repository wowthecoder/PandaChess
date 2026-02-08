#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>
#include <climits>

namespace panda {

// ============================================================
// Move ordering
// ============================================================

static constexpr int TT_MOVE_SCORE    = 10000000;
static constexpr int CAPTURE_BASE     =  1000000;
static constexpr int KILLER1_SCORE    =   900000;
static constexpr int KILLER2_SCORE    =   800000;

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
// Negamax with alpha-beta pruning
// ============================================================

static int negamax(const Board& board, int depth, int alpha, int beta,
                   SearchState& state, int ply) {
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

    // Base case: quiescence search
    if (depth == 0)
        return quiescence(board, alpha, beta, state);

    MoveList moves = generate_legal(board);

    // Terminal node detection
    if (moves.size() == 0) {
        if (in_check(board))
            return -MATE_SCORE + ply; // Checkmate
        return 0;                     // Stalemate
    }

    // 50-move rule draw
    if (is_draw_by_fifty_move_rule(board))
        return 0;

    // Score and order moves
    int scores[256];
    scoreMoves(board, moves, scores, ttMove, state, ply);

    Move bestMove = moves[0];
    TTFlag flag = TT_ALPHA;

    for (int i = 0; i < moves.size(); ++i) {
        pickBest(moves, scores, i);
        Move m = moves[i];

        Board child = board;
        child.make_move(m);

        int score = -negamax(child, depth - 1, -beta, -alpha, state, ply + 1);

        if (state.stopped) return 0;

        if (score >= beta) {
            state.tt.store(board.hash_key(), score, depth, TT_BETA, m);

            // Update killer moves and history for quiet moves
            if (!isCapture(board, m)) {
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

static SearchResult searchRoot(const Board& board, int depth, SearchState& state) {
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
    int alpha = -MATE_SCORE - 1;
    int beta = MATE_SCORE + 1;

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
        SearchResult result = searchRoot(board, depth, state);
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

    return searchRoot(board, depth, state);
}

} // namespace panda
