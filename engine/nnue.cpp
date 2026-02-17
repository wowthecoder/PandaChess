#include "nnue.h"

#include "eval.h"
#include "nnue/panda_nnue.h"

namespace panda {

int evaluate_nnue(const Board& board) {
    if (!nnue::backend_loaded())
        return evaluate_handcrafted(board);

    nnue::SearchNnueContext ctx;
    if (!ctx.is_available())
        return evaluate_handcrafted(board);

    ctx.reset(board);
    return ctx.evaluate(board);
}

int evaluate_nnue(const Board& board, nnue::SearchNnueContext* ctx) {
    if (!nnue::backend_loaded())
        return evaluate_handcrafted(board);

    if (!ctx)
        return evaluate_nnue(board);

    return ctx->evaluate(board);
}

bool nnue_backend_ready() {
    return nnue::backend_loaded();
}

}  // namespace panda
