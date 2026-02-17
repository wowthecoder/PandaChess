#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "../attacks.h"
#include "../board.h"
#include "../nnue.h"
#include "../zobrist.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace panda;

const int TOLERANCE = 20;

class NnueStockfishTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        zobrist::init();
        attacks::init();
    }
};

static auto* env = ::testing::AddGlobalTestEnvironment(new NnueStockfishTestEnvironment);

#if defined(__unix__) || defined(__APPLE__)

namespace {

std::optional<std::string> find_stockfish_executable() {
    std::vector<std::string> candidates;

    if (const char* fromEnv = std::getenv("PANDA_STOCKFISH_BIN"); fromEnv && *fromEnv)
        candidates.emplace_back(fromEnv);

#ifdef PANDA_ENGINE_SOURCE_DIR
    candidates.emplace_back(std::string(PANDA_ENGINE_SOURCE_DIR) + "/stockfish-18");
#endif
    candidates.emplace_back("stockfish-18");
    candidates.emplace_back("../stockfish-18");
    candidates.emplace_back("../../stockfish-18");
    candidates.emplace_back("engine/stockfish-18");

    for (const std::string& path : candidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            continue;
        if (access(path.c_str(), X_OK) == 0)
            return path;
    }
    return std::nullopt;
}

bool write_all(int fd, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        ssize_t n = write(fd, data.data() + offset, data.size() - offset);
        if (n <= 0)
            return false;
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

std::optional<std::string> run_stockfish(const std::string& executable, const std::string& input) {
    int stdinPipe[2];
    int stdoutPipe[2];
    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0)
        return std::nullopt;

    pid_t pid = fork();
    if (pid < 0)
        return std::nullopt;

    if (pid == 0) {
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);

        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);

#ifdef PANDA_ENGINE_SOURCE_DIR
        const int chdirResult = chdir(PANDA_ENGINE_SOURCE_DIR);
        (void)chdirResult;
#endif

        execl(executable.c_str(), executable.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    if (!write_all(stdinPipe[1], input)) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        return std::nullopt;
    }
    close(stdinPipe[1]);

    std::string output;
    char buffer[4096];
    while (true) {
        ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer));
        if (n <= 0)
            break;
        output.append(buffer, static_cast<std::size_t>(n));
    }
    close(stdoutPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return std::nullopt;

    return output;
}

std::optional<double> stockfish_final_eval_white_pawns(const std::string& executable,
                                                       const std::string& fen) {
    const std::string input =
        "uci\nsetoption name Threads value 1\nisready\nposition fen " + fen + "\neval\nquit\n";

    std::optional<std::string> output = run_stockfish(executable, input);
    if (!output)
        return std::nullopt;

    static const std::regex pattern(R"(Final evaluation\s+([+-]?\d+(?:\.\d+)?)\s+\(white side\))");
    std::smatch match;
    if (!std::regex_search(*output, match, pattern))
        return std::nullopt;

    try {
        return std::stod(match[1].str());
    } catch (...) {
        return std::nullopt;
    }
}

int stockfish_material_count(const Board& board) {
    return popcount(board.pieces(White, Pawn)) + popcount(board.pieces(Black, Pawn)) +
           3 * (popcount(board.pieces(White, Knight)) + popcount(board.pieces(Black, Knight))) +
           3 * (popcount(board.pieces(White, Bishop)) + popcount(board.pieces(Black, Bishop))) +
           5 * (popcount(board.pieces(White, Rook)) + popcount(board.pieces(Black, Rook))) +
           9 * (popcount(board.pieces(White, Queen)) + popcount(board.pieces(Black, Queen)));
}

double stockfish_to_cp_scale_a(const Board& board) {
    // Matches Stockfish uci.cpp::win_rate_params() polynomial.
    const int material = std::clamp(stockfish_material_count(board), 17, 78);
    const double m = double(material) / 58.0;

    constexpr double as[] = {-72. 2565836, 185.93832038, -144.58862193, 416.44950446};
    return (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
}

int panda_internal_value_to_stockfish_cp(int value, const Board& board) {
    const double a = stockfish_to_cp_scale_a(board);
    return static_cast<int>(std::lround(100.0 * value / a));
}

}  // namespace

TEST(NnueStockfishParityTest, PandaNnueIsSimilarToStockfishEval) {
    if (!nnue_backend_ready())
        GTEST_SKIP() << "Panda NNUE backend not available";

    const auto stockfishPath = find_stockfish_executable();
    if (!stockfishPath)
        GTEST_SKIP() << "Stockfish executable not found. Set PANDA_STOCKFISH_BIN or place "
                        "stockfish-18 in engine/";

    struct TestCase {
        const char* fen;
        int toleranceCp;
    };

    const std::vector<TestCase> cases = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", TOLERANCE},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", TOLERANCE},
        {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 3", TOLERANCE},
        {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq - 2 3", TOLERANCE},
        {"r1bq1rk1/pp1n1pbp/2pp1np1/2p5/2BPP3/2N2N2/PPQ2PPP/R1B2RK1 w - - 0 10", TOLERANCE},
        {"r2q1rk1/1bp1bppp/p1np1n2/1p2p3/3PP3/1BN1BN2/PPPQ1PPP/2KR3R w - - 0 11", TOLERANCE},
        {"r2qr1k1/pp1bbppp/2np1n2/2p1p3/2P1P3/2NPBN2/PPQ1BPPP/R4RK1 w - - 4 12", TOLERANCE},
        {"r2qr1k1/pp1bbppp/2np1n2/2p1p3/2P1P3/2NPBN2/PPQ1BPPP/R4RK1 b - - 4 12", TOLERANCE},
    };

    for (const TestCase& tc : cases) {
        Board board;
        board.set_fen(tc.fen);

        const int pandaValue = evaluate_nnue(board);
        const int pandaStmCp = panda_internal_value_to_stockfish_cp(pandaValue, board);

        std::optional<double> sfWhitePawns =
            stockfish_final_eval_white_pawns(*stockfishPath, tc.fen);
        ASSERT_TRUE(sfWhitePawns.has_value())
            << "Failed to parse Stockfish eval for FEN: " << tc.fen;

        int sfWhiteCp = static_cast<int>(std::lround(*sfWhitePawns * 100.0));
        int sfStmCp = (board.side_to_move() == White) ? sfWhiteCp : -sfWhiteCp;
        int absDiff = std::abs(pandaStmCp - sfStmCp);

        EXPECT_LE(absDiff, tc.toleranceCp)
            << "FEN: " << tc.fen << " pandaValue=" << pandaValue << " pandaStmCp=" << pandaStmCp
            << " sfStmCp=" << sfStmCp << " absDiff=" << absDiff;
    }
}

#else

TEST(NnueStockfishParityTest, PandaNnueIsSimilarToStockfishEval) {
    GTEST_SKIP() << "This test requires a POSIX environment";
}

#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
