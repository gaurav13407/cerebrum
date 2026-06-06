#include "movegen.h"
#include <cassert>

// =============================================================================
// ATTACK TABLE STORAGE
//
//   Defined here (in the .cpp), declared extern in movegen.h.
//   Only one copy exists in the entire program.
// =============================================================================

Bitboard knightAttacks[64];
Bitboard kingAttacks[64];
Bitboard pawnAttacks[2][64];

// =============================================================================
// PRECOMPUTED RAY TABLES  (for hyperbola quintessence)
//
//   For each square and each of the 4 sliding directions, store a bitboard
//   of all squares along that ray (not including the origin square itself).
//
//   Directions:
//     FILE  = vertical   (rook)
//     RANK  = horizontal (rook)
//     DIAG  = diagonal   (bishop, top-right to bottom-left)
//     ANTI  = antidiag   (bishop, top-left to bottom-right)
// =============================================================================

static Bitboard rayFile[64];
static Bitboard rayRank[64];
static Bitboard rayDiag[64];
static Bitboard rayAnti[64];

// =============================================================================
// initAttackTables
//
//   Called ONCE at startup.
//   Fills knightAttacks, kingAttacks, pawnAttacks, and all ray tables.
// =============================================================================

void initAttackTables() {

    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;   // 0..7
        int file = sq % 8;   // 0..7

        // ---- KNIGHT ATTACKS ----
        // A knight on sq can jump to up to 8 squares.
        // We check each of the 8 (drank, dfile) offsets and skip
        // any that go off the board.
        Bitboard kn = 0;
        const int knightDr[] = { 2,  2, -2, -2,  1,  1, -1, -1};
        const int knightDf[] = { 1, -1,  1, -1,  2, -2,  2, -2};
        for (int i = 0; i < 8; i++) {
            int r = rank + knightDr[i];
            int f = file + knightDf[i];
            if (r >= 0 && r < 8 && f >= 0 && f < 8)
                kn |= (1ULL << (r * 8 + f));
        }
        knightAttacks[sq] = kn;

        // ---- KING ATTACKS ----
        // A king on sq can move to up to 8 adjacent squares.
        Bitboard kg = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;  // skip the square itself
                int r = rank + dr;
                int f = file + df;
                if (r >= 0 && r < 8 && f >= 0 && f < 8)
                    kg |= (1ULL << (r * 8 + f));
            }
        }
        kingAttacks[sq] = kg;

        // ---- PAWN ATTACKS ----
        // White pawns attack diagonally upward (rank+1).
        // Black pawns attack diagonally downward (rank-1).
        Bitboard wp = 0, bp = 0;
        if (rank + 1 < 8) {
            if (file - 1 >= 0) wp |= (1ULL << ((rank + 1) * 8 + file - 1));
            if (file + 1 <  8) wp |= (1ULL << ((rank + 1) * 8 + file + 1));
        }
        if (rank - 1 >= 0) {
            if (file - 1 >= 0) bp |= (1ULL << ((rank - 1) * 8 + file - 1));
            if (file + 1 <  8) bp |= (1ULL << ((rank - 1) * 8 + file + 1));
        }
        pawnAttacks[WHITE][sq] = wp;
        pawnAttacks[BLACK][sq] = bp;

        // ---- RAY TABLES ----
        // For each direction, set all bits along that ray from sq.

        // FILE ray: all squares on the same file, above and below
        Bitboard rf = 0;
        for (int r = 0; r < 8; r++)
            if (r != rank) rf |= (1ULL << (r * 8 + file));
        rayFile[sq] = rf;

        // RANK ray: all squares on the same rank, left and right
        Bitboard rr = 0;
        for (int f = 0; f < 8; f++)
            if (f != file) rr |= (1ULL << (rank * 8 + f));
        rayRank[sq] = rr;

        // DIAGONAL ray: top-right to bottom-left (a1-h8 direction)
        Bitboard rd = 0;
        for (int d = -7; d <= 7; d++) {
            if (d == 0) continue;
            int r = rank + d, f = file + d;
            if (r >= 0 && r < 8 && f >= 0 && f < 8)
                rd |= (1ULL << (r * 8 + f));
        }
        rayDiag[sq] = rd;

        // ANTIDIAGONAL ray: top-left to bottom-right (h1-a8 direction)
        Bitboard ra = 0;
        for (int d = -7; d <= 7; d++) {
            if (d == 0) continue;
            int r = rank + d, f = file - d;
            if (r >= 0 && r < 8 && f >= 0 && f < 8)
                ra |= (1ULL << (r * 8 + f));
        }
        rayAnti[sq] = ra;
    }
}

// =============================================================================
// HYPERBOLA QUINTESSENCE  (sliding piece attack computation)
//
//   Given a sliding piece on 'sq' and the set of all occupied squares,
//   compute which squares it can attack along one ray direction.
//
//   The trick: o^(o-2s) computes attacks in the "positive" direction of a ray.
//   We do it twice (once forward, once with bits reversed for backward) and OR.
//
//   This is the standard efficient approach before magic bitboards.
//   Reference: chessprogramming.org/Hyperbola_Quintessence
// =============================================================================

// Reverse all 64 bits of a bitboard (used for the backward ray direction)
static inline Bitboard reverse(Bitboard b) {
    b = ((b >> 1)  & 0x5555555555555555ULL) | ((b & 0x5555555555555555ULL) << 1);
    b = ((b >> 2)  & 0x3333333333333333ULL) | ((b & 0x3333333333333333ULL) << 2);
    b = ((b >> 4)  & 0x0F0F0F0F0F0F0F0FULL) | ((b & 0x0F0F0F0F0F0F0F0FULL) << 4);
    b = ((b >> 8)  & 0x00FF00FF00FF00FFULL) | ((b & 0x00FF00FF00FF00FFULL) << 8);
    b = ((b >> 16) & 0x0000FFFF0000FFFFULL) | ((b & 0x0000FFFF0000FFFFULL) << 16);
    b =  (b >> 32)                          |  (b                           << 32);
    return b;
}

// Core hyperbola quintessence formula for one ray mask
static inline Bitboard hyperbolaAttacks(int sq, Bitboard occ, Bitboard mask) {
    Bitboard s   = 1ULL << sq;
    Bitboard fwd = (occ & mask) - 2 * s;
    Bitboard rev = reverse(reverse(occ & mask) - 2 * reverse(s));
    return (fwd ^ rev) & mask;
}

// Rook attacks = file attacks OR rank attacks
Bitboard rookAttacks(int sq, Bitboard occ) {
    return hyperbolaAttacks(sq, occ, rayFile[sq])
         | hyperbolaAttacks(sq, occ, rayRank[sq]);
}

// Bishop attacks = diagonal attacks OR antidiagonal attacks
Bitboard bishopAttacks(int sq, Bitboard occ) {
    return hyperbolaAttacks(sq, occ, rayDiag[sq])
         | hyperbolaAttacks(sq, occ, rayAnti[sq]);
}

// =============================================================================
// isSquareAttacked
//
//   Returns true if square 'sq' is attacked by any piece of color 'attacker'.
//
//   Strategy: stand on sq, pretend to be each piece type,
//   and check if any real piece of that type can see you.
//   This is the standard "reverse attack" technique.
// =============================================================================

bool isSquareAttacked(const Board& board, int sq, Color attacker) {
    // Is sq attacked by a pawn?
    // A white pawn on sq would attack certain squares — if a real white pawn
    // sits on any of those squares, sq is attacked.
    if (pawnAttacks[!attacker][sq] & board.bitboards[attacker][PAWN])
        return true;

    // Is sq attacked by a knight?
    if (knightAttacks[sq] & board.bitboards[attacker][KNIGHT])
        return true;

    // Is sq attacked by a king?
    if (kingAttacks[sq] & board.bitboards[attacker][KING])
        return true;

    // Is sq attacked by a rook or queen (on file/rank)?
    if (rookAttacks(sq, board.occupancyAll)
        & (board.bitboards[attacker][ROOK] | board.bitboards[attacker][QUEEN]))
        return true;

    // Is sq attacked by a bishop or queen (on diagonal)?
    if (bishopAttacks(sq, board.occupancyAll)
        & (board.bitboards[attacker][BISHOP] | board.bitboards[attacker][QUEEN]))
        return true;

    return false;
}

// =============================================================================
// LEGALITY FILTER
//
//   After generating a pseudo-legal move, we make it on the board and check
//   if our own king is left in check. If yes, the move is illegal — discard it.
//
//   This is the simplest correct approach. We'll optimise with pinned-piece
//   detection later.
// =============================================================================

// Make a move on the board (temporary, for legality checking)
// Returns the captured piece (EMPTY if none) so we can undo.
static Piece makeMoveLegal(Board& board, Move m) {
    int from  = moveFrom(m);
    int to    = moveTo(m);
    uint32_t flags = moveFlags(m);

    Piece captured = board.mailbox[to];

    if (flags == FLAG_EN_PASSANT) {
        // Captured pawn is not on 'to' but one rank behind it
        int capSq = (board.sideToMove == WHITE) ? to - 8 : to + 8;
        captured = board.mailbox[capSq];
        board.removePiece(static_cast<Square>(capSq));
    } else if (captured != EMPTY) {
        board.removePiece(static_cast<Square>(to));
    }

    board.movePiece(static_cast<Square>(from), static_cast<Square>(to));

    // Handle promotions
    if (isPromotion(m)) {
        PieceType promoPt;
        switch (movePromo(m)) {
            case PROMO_KNIGHT: promoPt = KNIGHT; break;
            case PROMO_BISHOP: promoPt = BISHOP; break;
            case PROMO_ROOK:   promoPt = ROOK;   break;
            default:           promoPt = QUEEN;  break;
        }
        board.removePiece(static_cast<Square>(to));
        board.putPiece(makePiece(board.sideToMove, promoPt), static_cast<Square>(to));
    }

    // Handle castling — move the rook
    if (flags == FLAG_CASTLE_K) {
        if (board.sideToMove == WHITE) board.movePiece(H1, F1);
        else                           board.movePiece(H8, F8);
    } else if (flags == FLAG_CASTLE_Q) {
        if (board.sideToMove == WHITE) board.movePiece(A1, D1);
        else                           board.movePiece(A8, D8);
    }

    return captured;
}

// Undo the move made by makeMoveLegal
static void undoMoveLegal(Board& board, Move m, Piece captured) {
    int from  = moveFrom(m);
    int to    = moveTo(m);
    uint32_t flags = moveFlags(m);

    // Undo castling rook first
    if (flags == FLAG_CASTLE_K) {
        if (board.sideToMove == WHITE) board.movePiece(F1, H1);
        else                           board.movePiece(F8, H8);
    } else if (flags == FLAG_CASTLE_Q) {
        if (board.sideToMove == WHITE) board.movePiece(D1, A1);
        else                           board.movePiece(D8, A8);
    }

    // Undo promotion: replace promoted piece with pawn
    if (isPromotion(m)) {
        board.removePiece(static_cast<Square>(to));
        board.putPiece(makePiece(board.sideToMove, PAWN), static_cast<Square>(to));
    }

    // Move piece back
    board.movePiece(static_cast<Square>(to), static_cast<Square>(from));

    // Restore captured piece
    if (flags == FLAG_EN_PASSANT) {
        int capSq = (board.sideToMove == WHITE) ? to - 8 : to + 8;
        board.putPiece(captured, static_cast<Square>(capSq));
    } else if (captured != EMPTY) {
        board.putPiece(captured, static_cast<Square>(to));
    }
}

// Check if a pseudo-legal move leaves our king in check (illegal if true)
static bool leavesKingInCheck(Board& board, Move m) {
    Piece captured = makeMoveLegal(board, m);
    int kingSq = __builtin_ctzll(board.bitboards[board.sideToMove][KING]);
    bool inCheck = isSquareAttacked(board, kingSq, !board.sideToMove);
    undoMoveLegal(board, m, captured);
    return inCheck;
}

// =============================================================================
// PAWN MOVE GENERATION
//
//   Handles: single push, double push, captures, en passant, promotions.
//   White pawns move up (toward rank 8), black pawns move down (toward rank 1).
// =============================================================================

void generatePawnMoves(const Board& board, MoveList& list) {
    Color us   = board.sideToMove;
    Color them = !us;

    Bitboard pawns    = board.bitboards[us][PAWN];
    Bitboard empty    = ~board.occupancyAll;
    Bitboard enemies  = board.occupancy[them];

    // Direction: white moves +8 (up), black moves -8 (down)
    int push  = (us == WHITE) ?  8 : -8;
    int rank7 = (us == WHITE) ?  6 :  1;   // rank index where pawns promote next move
    int rank2 = (us == WHITE) ?  1 :  6;   // rank index of starting rank (double push)

    Bitboard bb = pawns;
    while (bb) {
        int from = popLSB(bb);
        int fromRank = from / 8;

        // ---- SINGLE PUSH ----
        int to = from + push;
        if (to >= 0 && to < 64 && (empty >> to & 1)) {
            if (fromRank == rank7) {
                // Promotion: generate all 4 promotion options
                for (uint32_t p : {PROMO_QUEEN, PROMO_ROOK, PROMO_BISHOP, PROMO_KNIGHT})
                    list.add(makeMove(from, to, FLAG_PROMO, p));
            } else {
                list.add(makeMove(from, to, FLAG_QUIET));
            }

            // ---- DOUBLE PUSH ---- (only from starting rank, only if single push was clear)
            if (fromRank == rank2) {
                int to2 = to + push;
                if (to2 >= 0 && to2 < 64 && (empty >> to2 & 1))
                    list.add(makeMove(from, to2, FLAG_DOUBLE_PUSH));
            }
        }

        // ---- CAPTURES ----
        Bitboard attacks = pawnAttacks[us][from] & enemies;
        while (attacks) {
            int capTo = popLSB(attacks);
            if (fromRank == rank7) {
                for (uint32_t p : {PROMO_QUEEN, PROMO_ROOK, PROMO_BISHOP, PROMO_KNIGHT})
                    list.add(makeMove(from, capTo, FLAG_PROMO_CAP, p));
            } else {
                list.add(makeMove(from, capTo, FLAG_CAPTURE));
            }
        }

        // ---- EN PASSANT ----
        if (board.enPassantSquare != NO_SQUARE) {
            if (pawnAttacks[us][from] & (1ULL << board.enPassantSquare))
                list.add(makeMove(from, board.enPassantSquare, FLAG_EN_PASSANT));
        }
    }
}

// =============================================================================
// KNIGHT MOVE GENERATION
//
//   Simple: look up precomputed attack table, mask out friendly pieces.
// =============================================================================

void generateKnightMoves(const Board& board, MoveList& list) {
    Color us = board.sideToMove;
    Bitboard knights = board.bitboards[us][KNIGHT];
    Bitboard friends = board.occupancy[us];

    while (knights) {
        int from = popLSB(knights);
        // All squares the knight can reach, minus squares with our own pieces
        Bitboard targets = knightAttacks[from] & ~friends;
        while (targets) {
            int to = popLSB(targets);
            bool cap = board.mailbox[to] != EMPTY;
            list.add(makeMove(from, to, cap ? FLAG_CAPTURE : FLAG_QUIET));
        }
    }
}

// =============================================================================
// BISHOP MOVE GENERATION
// =============================================================================

void generateBishopMoves(const Board& board, MoveList& list) {
    Color us = board.sideToMove;
    Bitboard bishops = board.bitboards[us][BISHOP];
    Bitboard friends = board.occupancy[us];

    while (bishops) {
        int from = popLSB(bishops);
        Bitboard targets = bishopAttacks(from, board.occupancyAll) & ~friends;
        while (targets) {
            int to = popLSB(targets);
            bool cap = board.mailbox[to] != EMPTY;
            list.add(makeMove(from, to, cap ? FLAG_CAPTURE : FLAG_QUIET));
        }
    }
}

// =============================================================================
// ROOK MOVE GENERATION
// =============================================================================

void generateRookMoves(const Board& board, MoveList& list) {
    Color us = board.sideToMove;
    Bitboard rooks   = board.bitboards[us][ROOK];
    Bitboard friends = board.occupancy[us];

    while (rooks) {
        int from = popLSB(rooks);
        Bitboard targets = rookAttacks(from, board.occupancyAll) & ~friends;
        while (targets) {
            int to = popLSB(targets);
            bool cap = board.mailbox[to] != EMPTY;
            list.add(makeMove(from, to, cap ? FLAG_CAPTURE : FLAG_QUIET));
        }
    }
}

// =============================================================================
// QUEEN MOVE GENERATION
// =============================================================================

void generateQueenMoves(const Board& board, MoveList& list) {
    Color us = board.sideToMove;
    Bitboard queens  = board.bitboards[us][QUEEN];
    Bitboard friends = board.occupancy[us];

    while (queens) {
        int from = popLSB(queens);
        Bitboard targets = queenAttacks(from, board.occupancyAll) & ~friends;
        while (targets) {
            int to = popLSB(targets);
            bool cap = board.mailbox[to] != EMPTY;
            list.add(makeMove(from, to, cap ? FLAG_CAPTURE : FLAG_QUIET));
        }
    }
}

// =============================================================================
// KING MOVE GENERATION
//
//   Normal moves + castling.
//   Castling conditions:
//     1. The castling right flag must still be set
//     2. Squares between king and rook must be empty
//     3. King must not pass through or land on an attacked square
//     4. King must not currently be in check
// =============================================================================

void generateKingMoves(const Board& board, MoveList& list) {
    Color us = board.sideToMove;
    Bitboard friends = board.occupancy[us];
    int from = __builtin_ctzll(board.bitboards[us][KING]);

    // ---- NORMAL KING MOVES ----
    Bitboard targets = kingAttacks[from] & ~friends;
    while (targets) {
        int to = popLSB(targets);
        bool cap = board.mailbox[to] != EMPTY;
        list.add(makeMove(from, to, cap ? FLAG_CAPTURE : FLAG_QUIET));
    }

    // ---- CASTLING ----
    // Don't castle while in check
    if (inCheck(board)) return;

    if (us == WHITE) {
        // White kingside (E1-G1): squares F1, G1 must be empty
        if ((board.castlingRights & WHITE_OO)
            && !(board.occupancyAll & 0x60ULL)              // F1=bit5, G1=bit6
            && !isSquareAttacked(board, F1, BLACK)
            && !isSquareAttacked(board, G1, BLACK))
            list.add(makeMove(E1, G1, FLAG_CASTLE_K));

        // White queenside (E1-C1): squares B1, C1, D1 must be empty
        if ((board.castlingRights & WHITE_OOO)
            && !(board.occupancyAll & 0xEULL)               // B1=bit1, C1=bit2, D1=bit3
            && !isSquareAttacked(board, D1, BLACK)
            && !isSquareAttacked(board, C1, BLACK))
            list.add(makeMove(E1, C1, FLAG_CASTLE_Q));

    } else {
        // Black kingside (E8-G8)
        if ((board.castlingRights & BLACK_OO)
            && !(board.occupancyAll & 0x6000000000000000ULL)
            && !isSquareAttacked(board, F8, WHITE)
            && !isSquareAttacked(board, G8, WHITE))
            list.add(makeMove(E8, G8, FLAG_CASTLE_K));

        // Black queenside (E8-C8)
        if ((board.castlingRights & BLACK_OOO)
            && !(board.occupancyAll & 0xE00000000000000ULL)
            && !isSquareAttacked(board, D8, WHITE)
            && !isSquareAttacked(board, C8, WHITE))
            list.add(makeMove(E8, C8, FLAG_CASTLE_Q));
    }
}

// =============================================================================
// generateMoves — master function
//
//   Generates all pseudo-legal moves, then filters out moves that leave
//   our king in check. Result is a list of fully legal moves.
// =============================================================================

void generateMoves(const Board& board, MoveList& list) {
    // Cast away const for the legality check (we make/undo moves temporarily)
    Board& b = const_cast<Board&>(board);

    MoveList pseudo;
    generatePawnMoves  (b, pseudo);
    generateKnightMoves(b, pseudo);
    generateBishopMoves(b, pseudo);
    generateRookMoves  (b, pseudo);
    generateQueenMoves (b, pseudo);
    generateKingMoves  (b, pseudo);

    // Filter: only keep moves that don't leave our king in check
    for (int i = 0; i < pseudo.count; i++) {
        if (!leavesKingInCheck(b, pseudo.moves[i]))
            list.add(pseudo.moves[i]);
    }
}

// =============================================================================
// generateCaptures — only capture moves (used in quiescence search)
// =============================================================================

void generateCaptures(const Board& board, MoveList& list) {
    Board& b = const_cast<Board&>(board);

    MoveList pseudo;
    generatePawnMoves  (b, pseudo);
    generateKnightMoves(b, pseudo);
    generateBishopMoves(b, pseudo);
    generateRookMoves  (b, pseudo);
    generateQueenMoves (b, pseudo);
    generateKingMoves  (b, pseudo);

    for (int i = 0; i < pseudo.count; i++) {
        if (isCapture(pseudo.moves[i]) && !leavesKingInCheck(b, pseudo.moves[i]))
            list.add(pseudo.moves[i]);
    }
}
