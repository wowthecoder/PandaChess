#include "uci.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "attacks.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "tt.h"
#include "zobrist.h"

namespace panda {

static const char* ENGINE_NAME = "PandaChess";
static const char* ENGINE_AUTHOR = "PandaChess Team";
static constexpr int MOVE_OVERHEAD_MS = 20;
static constexpr int MIN_SEARCH_MS = 1;

// Parse a UCI move string (e.g. "e2e4", "e7e8q") and match against legal moves
static Move parseUCIMove(const Board& board, const std::string& str) {
    if (str.size() < 4)
        return NullMove;

    int fromFile = str[0] - 'a';
    int fromRank = str[1] - '1';
    int toFile = str[2] - 'a';
    int toRank = str[3] - '1';

    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 || toFile < 0 || toFile > 7 ||
        toRank < 0 || toRank > 7)
        return NullMove;

    Square from = make_square(fromFile, fromRank);
    Square to = make_square(toFile, toRank);

    // Determine promotion piece if present
    PieceType promoPiece = Queen;  // default
    if (str.size() >= 5) {
        switch (str[4]) {
            case 'n':
                promoPiece = Knight;
                break;
            case 'b':
                promoPiece = Bishop;
                break;
            case 'r':
                promoPiece = Rook;
                break;
            case 'q':
                promoPiece = Queen;
                break;
        }
    }

    // Match against legal moves
    MoveList legal = generate_legal(board);
    for (int i = 0; i < legal.size(); ++i) {
        Move m = legal[i];
        if (move_from(m) == from && move_to(m) == to) {
            if (move_type(m) == Promotion) {
                if (promotion_type(m) == promoPiece)
                    return m;
            } else {
                return m;
            }
        }
    }

    return NullMove;
}

// Handle "position" command
static void parsePosition(Board& board, std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "startpos") {
        board.set_fen(StartFEN);
        iss >> token;  // consume "moves" if present
    } else if (token == "fen") {
        std::string fen;
        // FEN has 6 fields
        for (int i = 0; i < 6 && (iss >> token); ++i) {
            if (token == "moves")
                break;
            if (i > 0)
                fen += ' ';
            fen += token;
        }
        board.set_fen(fen);
        // token may already be "moves" from the loop above
        if (token != "moves")
            iss >> token;
    }

    // Apply moves if present
    if (token == "moves") {
        while (iss >> token) {
            Move m = parseUCIMove(board, token);
            if (m != NullMove) {
                board.make_move(m);
            }
        }
    }
}

// Handle "go" command
static void parseGoAndSearch(const Board& board, std::istringstream& iss, TranspositionTable& tt,
                             std::atomic<bool>& stopFlag, std::thread& searchThread) {
    int wtime = 0, btime = 0, winc = 0, binc = 0;
    int movetime = 0;
    int movestogo = 0;
    int depth = 0;
    bool infinite = false;

    std::string token;
    while (iss >> token) {
        if (token == "wtime")
            iss >> wtime;
        else if (token == "btime")
            iss >> btime;
        else if (token == "winc")
            iss >> winc;
        else if (token == "binc")
            iss >> binc;
        else if (token == "movetime")
            iss >> movetime;
        else if (token == "movestogo")
            iss >> movestogo;
        else if (token == "depth")
            iss >> depth;
        else if (token == "infinite")
            infinite = true;
    }

    // Calculate time to search
    int timeLimitMs = 0;
    if (movetime > 0) {
        // Keep safety overhead so we do not lose on GUI/OS scheduling latency.
        timeLimitMs = movetime - MOVE_OVERHEAD_MS;
        if (timeLimitMs < MIN_SEARCH_MS)
            timeLimitMs = MIN_SEARCH_MS;
    } else if (!infinite && (wtime > 0 || btime > 0)) {
        // Simple time management: use a share of remaining time + increment.
        int myTime = (board.side_to_move() == White) ? wtime : btime;
        int myInc = (board.side_to_move() == White) ? winc : binc;
        int divisor = (movestogo > 0) ? movestogo : 30;
        timeLimitMs = myTime / divisor + (myInc * 3) / 4;

        // Hard cap below remaining clock to preserve move-overhead safety.
        int maxTime = myTime - MOVE_OVERHEAD_MS;
        if (maxTime < MIN_SEARCH_MS)
            maxTime = MIN_SEARCH_MS;
        if (timeLimitMs > maxTime)
            timeLimitMs = maxTime;
        if (timeLimitMs < MIN_SEARCH_MS)
            timeLimitMs = MIN_SEARCH_MS;
    }
    // If infinite or no time control: timeLimitMs stays 0 (no limit)

    int maxDepth = (depth > 0) ? depth : MAX_PLY;

    // Copy the board for the search thread
    Board searchBoard = board;
    stopFlag.store(false, std::memory_order_relaxed);

    searchThread = std::thread([searchBoard, timeLimitMs, maxDepth, &tt, &stopFlag]() {
        auto infoCb = [&tt](const SearchInfo& info) {
            std::cout << "info depth " << info.depth;
            if (info.isMate) {
                std::cout << " score mate " << info.mateInPly;
            } else {
                std::cout << " score cp " << info.score;
            }
            std::cout << " nodes " << info.nodes;
            std::cout << " time " << info.timeMs;
            std::cout << " hashfull " << tt.hashfull_permille();
            if (info.timeMs > 0) {
                uint64_t nps = (info.nodes * 1000) / static_cast<uint64_t>(info.timeMs);
                std::cout << " nps " << nps;
            }
            if (!info.pv.empty()) {
                std::cout << " pv";
                for (Move m : info.pv) std::cout << " " << move_to_uci(m);
            }
            std::cout << std::endl;
        };

        SearchResult result = search(searchBoard, timeLimitMs, maxDepth, tt, stopFlag, infoCb);

        if (result.bestMove == NullMove) {
            std::cout << "bestmove 0000" << std::endl;
        } else {
            std::cout << "bestmove " << move_to_uci(result.bestMove) << std::endl;
        }
    });
}

void uci_loop() {
    // Initialize engine tables
    attacks::init();
    zobrist::init();

    Board board;
    board.set_fen(StartFEN);

    TranspositionTable tt(64);  // 64 MB default
    std::atomic<bool> stopFlag{false};
    std::thread searchThread;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name " << ENGINE_NAME << std::endl;
            std::cout << "id author " << ENGINE_AUTHOR << std::endl;
            std::cout << "option name Hash type spin default 64 min 1 max 4096" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            // Wait for any running search to finish
            if (searchThread.joinable()) {
                stopFlag.store(true, std::memory_order_relaxed);
                searchThread.join();
            }
            tt.clear();
            board.set_fen(StartFEN);
        } else if (cmd == "position") {
            parsePosition(board, iss);
        } else if (cmd == "go") {
            // Wait for any previous search to finish
            if (searchThread.joinable()) {
                stopFlag.store(true, std::memory_order_relaxed);
                searchThread.join();
            }
            parseGoAndSearch(board, iss, tt, stopFlag, searchThread);
        } else if (cmd == "stop") {
            stopFlag.store(true, std::memory_order_relaxed);
            if (searchThread.joinable())
                searchThread.join();
        } else if (cmd == "setoption") {
            std::string token;
            iss >> token;  // "name"
            std::string name;
            iss >> name;
            // Read multi-word option names
            while (iss >> token && token != "value") name += " " + token;
            std::string value;
            if (iss >> value) {
                if (name == "Hash") {
                    int sizeMB = std::stoi(value);
                    if (sizeMB < 1)
                        sizeMB = 1;
                    if (sizeMB > 4096)
                        sizeMB = 4096;
                    tt = TranspositionTable(static_cast<size_t>(sizeMB));
                }
            }
        } else if (cmd == "quit") {
            if (searchThread.joinable()) {
                stopFlag.store(true, std::memory_order_relaxed);
                searchThread.join();
            }
            break;
        }
    }

    // ADDED: Cleanup when exiting loop (EOF or error)
    if (searchThread.joinable()) {
        stopFlag.store(true, std::memory_order_relaxed);
        searchThread.join();
    }
}

}  // namespace panda
