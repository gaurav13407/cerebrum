#include "board.h"
#include "movegen.h"
#include <iostream>
#include <chrono>

// --------------------------------------------------------
// makeMove / undoMove  (full game state version)
// We need to save/restore castling rights, en passant, etc.
// --------------------------------------------------------

struct UndoInfo {
    uint8_t castlingRights;
    int     enPassantSquare;
    int     halfMoveClock;
    Piece   captured;
};

UndoInfo makeMove(Board& board, Move m) {
    UndoInfo undo;
    undo.castlingRights  = board.castlingRights;
    undo.enPassantSquare = board.enPassantSquare;
    undo.halfMoveClock   = board.halfMoveClock;
    undo.captured        = EMPTY;

    int from       = moveFrom(m);
    int to         = moveTo(m);
    uint32_t flags = moveFlags(m);
    Color us       = board.sideToMove;

    // Reset en passant every move
    board.enPassantSquare = NO_SQUARE;
    board.halfMoveClock++;

    if (flags == FLAG_EN_PASSANT) {
        int capSq = (us == WHITE) ? to - 8 : to + 8;
        undo.captured = board.mailbox[capSq];
        board.removePiece(static_cast<Square>(capSq));
        board.movePiece(static_cast<Square>(from), static_cast<Square>(to));
        board.halfMoveClock = 0;

    } else if (flags == FLAG_DOUBLE_PUSH) {
        board.movePiece(static_cast<Square>(from), static_cast<Square>(to));
        board.enPassantSquare = (us == WHITE) ? to - 8 : to + 8;
        board.halfMoveClock = 0;

    } else if (flags == FLAG_CASTLE_K) {
        board.movePiece(static_cast<Square>(from), static_cast<Square>(to));
        if (us == WHITE) board.movePiece(H1, F1);
        else             board.movePiece(H8, F8);

    } else if (flags == FLAG_CASTLE_Q) {
        board.movePiece(static_cast<Square>(from), static_cast<Square>(to));
        if (us == WHITE) board.movePiece(A1, D1);
        else             board.movePiece(A8, D8);

    } else {
        // Normal move or capture
        undo.captured = board.mailbox[to];
        if (undo.captured != EMPTY) {
            board.removePiece(static_cast<Square>(to));
            board.halfMoveClock = 0;
        }
        board.movePiece(static_cast<Square>(from), static_cast<Square>(to));

        // Pawn move resets clock
        if (board.mailbox[to] == makePiece(us, PAWN))
            board.halfMoveClock = 0;
    }

    // Handle promotion
    if (isPromotion(m)) {
        PieceType pt;
        switch (movePromo(m)) {
            case PROMO_KNIGHT: pt = KNIGHT; break;
            case PROMO_BISHOP: pt = BISHOP; break;
            case PROMO_ROOK:   pt = ROOK;   break;
            default:           pt = QUEEN;  break;
        }
        board.removePiece(static_cast<Square>(to));
        board.putPiece(makePiece(us, pt), static_cast<Square>(to));
        board.halfMoveClock = 0;
    }

    // Update castling rights based on what moved
    // If king moved, lose both rights for that side
    if (board.mailbox[to] == makePiece(us, KING) ||
        from == E1 || to == E1) board.castlingRights &= ~(WHITE_OO | WHITE_OOO);
    if (board.mailbox[to] == makePiece(us, KING) ||
        from == E8 || to == E8) board.castlingRights &= ~(BLACK_OO | BLACK_OOO);
    // If a rook moved or was captured, lose that specific right
    if (from == H1 || to == H1) board.castlingRights &= ~WHITE_OO;
    if (from == A1 || to == A1) board.castlingRights &= ~WHITE_OOO;
    if (from == H8 || to == H8) board.castlingRights &= ~BLACK_OO;
    if (from == A8 || to == A8) board.castlingRights &= ~BLACK_OOO;

    board.sideToMove  = !us;
    board.fullMoveNumber += (us == BLACK);

    return undo;
}

void undoMove(Board& board, Move m, UndoInfo& undo) {
    int from       = moveFrom(m);
    int to         = moveTo(m);
    uint32_t flags = moveFlags(m);

    // Flip side back
    board.sideToMove = !board.sideToMove;
    Color us = board.sideToMove;

    // Undo castling rook
    if (flags == FLAG_CASTLE_K) {
        if (us == WHITE) board.movePiece(F1, H1);
        else             board.movePiece(F8, H8);
    } else if (flags == FLAG_CASTLE_Q) {
        if (us == WHITE) board.movePiece(D1, A1);
        else             board.movePiece(D8, A8);
    }

    // Undo promotion: put pawn back
    if (isPromotion(m)) {
        board.removePiece(static_cast<Square>(to));
        board.putPiece(makePiece(us, PAWN), static_cast<Square>(to));
    }

    // Move piece back
    board.movePiece(static_cast<Square>(to), static_cast<Square>(from));

    // Restore captured piece
    if (flags == FLAG_EN_PASSANT) {
        int capSq = (us == WHITE) ? to - 8 : to + 8;
        board.putPiece(undo.captured, static_cast<Square>(capSq));
    } else if (undo.captured != EMPTY) {
        board.putPiece(undo.captured, static_cast<Square>(to));
    }

    // Restore saved state
    board.castlingRights  = undo.castlingRights;
    board.enPassantSquare = undo.enPassantSquare;
    board.halfMoveClock   = undo.halfMoveClock;
    board.fullMoveNumber -= (us == BLACK);
}

// --------------------------------------------------------
// Perft — count leaf nodes at exactly 'depth'
// --------------------------------------------------------
uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;

    MoveList list;
    generateMoves(board, list);

    if (depth == 1) return list.count;  // optimisation: skip make/undo at depth 1

    uint64_t nodes = 0;
    for (int i = 0; i < list.count; i++) {
        UndoInfo undo = makeMove(board, list.moves[i]);
        nodes += perft(board, depth - 1);
        undoMove(board, list.moves[i], undo);
    }
    return nodes;
}

// Divide: show per-move node counts (helps pinpoint bugs)
void perftDivide(Board& board, int depth) {
    MoveList list;
    generateMoves(board, list);

    uint64_t total = 0;
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        UndoInfo undo = makeMove(board, m);
        uint64_t nodes = perft(board, depth - 1);
        undoMove(board, m, undo);

        // Print move in algebraic notation
        auto sqName = [](int sq) -> std::string {
            std::string s;
            s += (char)('a' + sq % 8);
            s += (char)('1' + sq / 8);
            return s;
        };
        std::string mv = sqName(moveFrom(m)) + sqName(moveTo(m));
        if (isPromotion(m)) {
            const char pc[] = {'n','b','r','q'};
            mv += pc[movePromo(m)];
        }

        std::cout << mv << ": " << nodes << "\n";
        total += nodes;
    }
    std::cout << "\nTotal: " << total << "\n";
}

int main() {
    initAttackTables();
    Board board;

    // Known correct perft values for starting position
    const uint64_t expected[] = {0, 20, 400, 8902, 197281, 4865609};

    std::cout << "=== Perft Test (Starting Position) ===\n\n";

    for (int depth = 1; depth <= 5; depth++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        uint64_t nodes = perft(board, depth);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        bool ok = (nodes == expected[depth]);
        std::cout << "depth " << depth
                  << "  nodes=" << nodes
                  << "  expected=" << expected[depth]
                  << "  " << (ok ? "OK" : "FAIL")
                  << "  (" << ms << " ms)\n";
    }

    std::cout << "\n=== Perft Divide depth 2 ===\n";
    perftDivide(board, 2);

    return 0;
}
