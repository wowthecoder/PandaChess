#include "search.h"
#include "eval.h"
#include "movegen.h"
#include "tt.h"
#include <algorithm>

namespace panda {

static int quiescence(const Board& board, int alpha, int beta) {
    int standPat = evaluate(board);

    if (standPat >= beta)
        return beta;
    if (standPat > alpha)
        alpha = standPat;

    MoveList moves = generate_legal(board);

    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];

        // Only search captures: the target square has an enemy piece, or en passant
        Square to = move_to(m);
        bool isCapture = (board.piece_on(to) != NoPiece) || (move_type(m) == EnPassant);
        if (!isCapture)
            continue;

        Board child = board;
        child.make_move(m);

        int score = -quiescence(child, -beta, -alpha);

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

static int negamax(const Board& board, int depth, int alpha, int beta,
                   TranspositionTable& tt, int ply) {
    // TT probe
    TTEntry ttEntry;
    if (tt.probe(board.hash_key(), ttEntry) && ttEntry.depth >= depth) {
        if (ttEntry.flag == TT_EXACT)
            return ttEntry.score;
        if (ttEntry.flag == TT_BETA && ttEntry.score >= beta)
            return ttEntry.score;
        if (ttEntry.flag == TT_ALPHA && ttEntry.score <= alpha)
            return ttEntry.score;
    }

    // Base case: quiescence search
    if (depth == 0)
        return quiescence(board, alpha, beta);

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

    // TT move ordering: try TT best move first by swapping it to front
    bool hasTTMove = tt.probe(board.hash_key(), ttEntry) && ttEntry.bestMove != NullMove;
    if (hasTTMove) {
        for (int i = 0; i < moves.size(); ++i) {
            if (moves.moves[i] == ttEntry.bestMove) {
                std::swap(moves.moves[0], moves.moves[i]);
                break;
            }
        }
    }

    Move bestMove = moves[0];
    TTFlag flag = TT_ALPHA;

    for (int i = 0; i < moves.size(); ++i) {
        Board child = board;
        child.make_move(moves[i]);

        int score = -negamax(child, depth - 1, -beta, -alpha, tt, ply + 1);

        if (score >= beta) {
            tt.store(board.hash_key(), score, depth, TT_BETA, moves[i]);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            bestMove = moves[i];
            flag = TT_EXACT;
        }
    }

    tt.store(board.hash_key(), alpha, depth, flag, bestMove);
    return alpha;
}

SearchResult search(const Board& board, int depth, TranspositionTable& tt) {
    MoveList moves = generate_legal(board);

    if (moves.size() == 0)
        return {NullMove, 0};

    Move bestMove = moves[0];
    int bestScore = -MATE_SCORE - 1;
    int alpha = -MATE_SCORE - 1;
    int beta = MATE_SCORE + 1;

    // TT move ordering at root
    TTEntry ttEntry;
    if (tt.probe(board.hash_key(), ttEntry) && ttEntry.bestMove != NullMove) {
        for (int i = 0; i < moves.size(); ++i) {
            if (moves.moves[i] == ttEntry.bestMove) {
                std::swap(moves.moves[0], moves.moves[i]);
                break;
            }
        }
    }

    for (int i = 0; i < moves.size(); ++i) {
        Board child = board;
        child.make_move(moves[i]);

        int score = -negamax(child, depth - 1, -beta, -alpha, tt, 1);

        if (score > bestScore) {
            bestScore = score;
            bestMove = moves[i];
        }
        if (score > alpha)
            alpha = score;
    }

    tt.store(board.hash_key(), bestScore, depth, TT_EXACT, bestMove);
    return {bestMove, bestScore};
}

} // namespace panda
