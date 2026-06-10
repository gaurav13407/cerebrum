#include "board.h"
#include "movegen.h"
#include "eval.h"
#include <iostream>
#include <string>
#include <iomanip>

// --------------------------------------------------------
// Minimal FEN loader so we can test specific positions
// --------------------------------------------------------
void loadFEN(Board& board, const std::string& fen) {
    // Zero everything
    for (int c = 0; c < NUM_COLORS; c++)
        for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
            board.bitboards[c][pt] = 0ULL;
    board.occupancy[WHITE] = board.occupancy[BLACK] = board.occupancyAll = 0ULL;
    for (int sq = 0; sq < NUM_SQUARES; sq++) board.mailbox[sq] = EMPTY;

    int i = 0;
    int rank = 7, file = 0;

    // 1. Piece placement
    while (fen[i] != ' ') {
        char c = fen[i++];
        if (c == '/') { rank--; file = 0; continue; }
        if (c >= '1' && c <= '8') { file += c - '0'; continue; }

        Color col = isupper(c) ? WHITE : BLACK;
        PieceType pt;
        switch (tolower(c)) {
            case 'p': pt = PAWN;   break;
            case 'n': pt = KNIGHT; break;
            case 'b': pt = BISHOP; break;
            case 'r': pt = ROOK;   break;
            case 'q': pt = QUEEN;  break;
            default:  pt = KING;   break;
        }
        board.putPiece(makePiece(col, pt), static_cast<Square>(rank * 8 + file));
        file++;
    }
    i++; // skip space

    // 2. Side to move
    board.sideToMove = (fen[i] == 'w') ? WHITE : BLACK;
    i += 2;

    // 3. Castling rights
    board.castlingRights = NO_CASTLING;
    while (fen[i] != ' ') {
        switch (fen[i++]) {
            case 'K': board.castlingRights |= WHITE_OO;  break;
            case 'Q': board.castlingRights |= WHITE_OOO; break;
            case 'k': board.castlingRights |= BLACK_OO;  break;
            case 'q': board.castlingRights |= BLACK_OOO; break;
        }
    }
    i++;

    // 4. En passant
    if (fen[i] == '-') {
        board.enPassantSquare = NO_SQUARE; i += 2;
    } else {
        int f = fen[i] - 'a';
        int r = fen[i+1] - '1';
        board.enPassantSquare = r * 8 + f;
        i += 3;
    }

    // 5. Halfmove clock
    board.halfMoveClock = 0;
    board.fullMoveNumber = 1;
}

// --------------------------------------------------------
// Test helper
// --------------------------------------------------------
void testEval(const std::string& label,
              const std::string& fen,
              const std::string& expectDesc) {
    Board board;
    loadFEN(board, fen);

    int score = evaluate(board);

    // Print result
    std::cout << "  " << std::left << std::setw(35) << label
              << "  score=" << std::setw(6) << score
              << "  expect: " << expectDesc << "\n";
}

int main() {
    initAttackTables();

    std::cout << "========================================\n";
    std::cout << " EVALUATION TESTS\n";
    std::cout << "========================================\n\n";

    // ---- 1. SYMMETRY TESTS ----
    // A symmetric position must always score 0
    std::cout << "[ Symmetry — must always be 0 ]\n";
    testEval("Starting position",
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             "= 0");
    testEval("Empty board (kings only)",
             "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
             "= 0");
    testEval("Symmetric pawn structure",
             "4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1",
             "= 0");
    std::cout << "\n";

    // ---- 2. MATERIAL TESTS ----
    // White up material → positive. Black up → negative.
    std::cout << "[ Material advantage ]\n";
    testEval("White up a queen",
             "4k3/8/8/8/8/8/8/4KQ2 w - - 0 1",
             "> 0 (white has queen)");
    testEval("Black up a rook",
             "3rk3/8/8/8/8/8/8/4K3 w - - 0 1",
             "< 0 (black has rook)");
    testEval("White up a pawn",
             "4k3/8/8/8/8/8/P7/4K3 w - - 0 1",
             "> 0 (white has pawn)");
    testEval("White up bishop pair vs nothing",
             "4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1",
             "> 0 (white has 2 bishops)");
    std::cout << "\n";

    // ---- 3. POSITIONAL TESTS ----
    // Knight in center vs corner
    std::cout << "[ Positional — piece placement ]\n";
    testEval("White knight center (e4)",
             "4k3/8/8/8/4N3/8/8/4K3 w - - 0 1",
             "> knight on a1 version");
    testEval("White knight corner (a1)",
             "4k3/8/8/8/8/8/8/N3K3 w - - 0 1",
             "< knight on e4 version");
    testEval("White rook on open e-file",
             "4k3/8/8/8/8/8/8/4KR2 w - - 0 1",
             "> 0");
    std::cout << "\n";

    // ---- 4. PAWN STRUCTURE TESTS ----
    std::cout << "[ Pawn structure ]\n";
    testEval("White has doubled pawns",
             "4k3/8/8/8/8/P7/P7/4K3 w - - 0 1",
             "penalized vs normal");
    testEval("White has passed pawn on rank 6",
             "4k3/8/P7/8/8/8/8/4K3 w - - 0 1",
             "> 0, big bonus");
    testEval("White has passed pawn on rank 7",
             "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
             "> 0, huge bonus");
    std::cout << "\n";

    // ---- 5. PERSPECTIVE TEST ----
    // Same position, different side to move → scores should be negations
    std::cout << "[ Perspective — side to move ]\n";
    Board b1, b2;
    std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e3 0 1";
    loadFEN(b1, fen);
    // Make it black to move same position
    std::string fen2 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    loadFEN(b2, fen2);
    int s1 = evaluate(b1);
    int s2 = evaluate(b2);
    std::cout << "  White to move score: " << s1 << "\n";
    std::cout << "  Black to move score: " << s2 << "\n";
    std::cout << "  Sum (should be 0):   " << s1 + s2 << " "
              << (s1 + s2 == 0 ? "OK" : "FAIL") << "\n";
    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << " Done.\n";
    std::cout << "========================================\n";

    return 0;
}
