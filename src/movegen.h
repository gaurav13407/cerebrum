#pragma once
#include "board.h"

// =============================================================================
// MOVE ENCODING
//
//   A move is a single uint32_t with this bit layout:
//
//   bits  0.. 5  → from square      (6 bits, 0..63)
//   bits  6..11  → to square        (6 bits, 0..63)
//   bits 12..13  → promotion piece  (2 bits)
//   bits 14..17  → move flags       (4 bits)
//
//   Encoding everything in one integer means:
//   - No heap allocation
//   - Fast copy (just an int)
//   - Easy to store in arrays
// =============================================================================

using Move = uint32_t;

// --- Promotion piece encoding (bits 12..13) ---
// When a pawn reaches the back rank it must promote.
// We encode which piece it promotes to in 2 bits.
constexpr uint32_t PROMO_KNIGHT = 0;   // 00
constexpr uint32_t PROMO_BISHOP = 1;   // 01
constexpr uint32_t PROMO_ROOK   = 2;   // 10
constexpr uint32_t PROMO_QUEEN  = 3;   // 11

// --- Move flags (bits 14..17) ---
// These tell us what kind of move this is.
constexpr uint32_t FLAG_QUIET        = 0b0000;  // normal move, no capture
constexpr uint32_t FLAG_DOUBLE_PUSH  = 0b0001;  // pawn moves two squares
constexpr uint32_t FLAG_CASTLE_K     = 0b0010;  // kingside castle  (O-O)
constexpr uint32_t FLAG_CASTLE_Q     = 0b0011;  // queenside castle (O-O-O)
constexpr uint32_t FLAG_CAPTURE      = 0b0100;  // normal capture
constexpr uint32_t FLAG_EN_PASSANT   = 0b0101;  // en passant capture
constexpr uint32_t FLAG_PROMO        = 0b1000;  // promotion, no capture
constexpr uint32_t FLAG_PROMO_CAP    = 0b1100;  // promotion + capture

// =============================================================================
// MOVE ENCODING / DECODING HELPERS
//
//   Always use these functions — never bit-shift manually in other files.
//   If we ever change the layout, we only fix it here.
// =============================================================================

// Build a move from its components
inline Move makeMove(int from, int to, uint32_t flags, uint32_t promo = PROMO_QUEEN) {
    return (flags << 14) | (promo << 12) | (to << 6) | from;
}

// Extract the from-square (bits 0..5)
inline int moveFrom(Move m) { return m & 0x3F; }

// Extract the to-square (bits 6..11)
inline int moveTo(Move m) { return (m >> 6) & 0x3F; }

// Extract the promotion piece (bits 12..13)
inline uint32_t movePromo(Move m) { return (m >> 12) & 0x3; }

// Extract the flags (bits 14..17)
inline uint32_t moveFlags(Move m) { return (m >> 14) & 0xF; }

// Convenience checks — use these in search/make-move code
inline bool isCapture(Move m)    { return moveFlags(m) & FLAG_CAPTURE; }
inline bool isPromotion(Move m)  { return moveFlags(m) & 0b1000; }
inline bool isEnPassant(Move m)  { return moveFlags(m) == FLAG_EN_PASSANT; }
inline bool isCastle(Move m)     { return moveFlags(m) == FLAG_CASTLE_K ||
                                          moveFlags(m) == FLAG_CASTLE_Q; }

// =============================================================================
// MOVE LIST
//
//   A fixed-size array on the stack.
//   Max legal moves in any chess position is 218.
//   We use 256 as a safe upper bound.
//
//   Why not std::vector?
//   - vector heap-allocates, which is slow when called millions of times
//   - This array lives on the stack: allocation is free (just move stack pointer)
// =============================================================================

constexpr int MAX_MOVES = 256;

struct MoveList {
    Move moves[MAX_MOVES];
    int  count = 0;

    // Add a move to the list
    inline void add(Move m) { moves[count++] = m; }

    // Iterators so we can use range-for: for (Move m : list)
    Move* begin() { return moves; }
    Move* end()   { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end()   const { return moves + count; }
};

// =============================================================================
// ATTACK TABLES
//
//   Precomputed at startup — one bitboard per square for knights/kings/pawns.
//   Sliding piece attacks are computed on the fly (they depend on blockers).
//
//   These are extern — defined once in movegen.cpp, used everywhere.
// =============================================================================

extern Bitboard knightAttacks[64];    // knightAttacks[sq] = all squares a knight on sq attacks
extern Bitboard kingAttacks[64];      // kingAttacks[sq]   = all squares a king on sq attacks
extern Bitboard pawnAttacks[2][64];   // pawnAttacks[color][sq] = squares a pawn attacks

// Call this ONCE at program startup before any move generation.
void initAttackTables();

// =============================================================================
// SLIDING PIECE ATTACKS  (hyperbola quintessence)
//
//   Computes rook/bishop attacks at runtime given the current occupancy.
//   'sq'        = square the piece is on
//   'occupancy' = all pieces on the board (blocks sliding)
// =============================================================================

Bitboard rookAttacks(int sq, Bitboard occupancy);
Bitboard bishopAttacks(int sq, Bitboard occupancy);

// Queen = rook + bishop combined
inline Bitboard queenAttacks(int sq, Bitboard occ) {
    return rookAttacks(sq, occ) | bishopAttacks(sq, occ);
}

// =============================================================================
// ATTACK DETECTION
//
//   Is square 'sq' attacked by any piece of color 'attacker'?
//   Used for: check detection, castling legality, king move legality.
// =============================================================================

bool isSquareAttacked(const Board& board, int sq, Color attacker);

// Is the side-to-move's king currently in check?
inline bool inCheck(const Board& board) {
    int kingSq = __builtin_ctzll(board.bitboards[board.sideToMove][KING]);
    return isSquareAttacked(board, kingSq, !board.sideToMove);
}

// =============================================================================
// MOVE GENERATION — main entry points
//
//   generateMoves()      — generates ALL legal moves (used in most cases)
//   generateCaptures()   — generates ONLY captures (used in quiescence search)
//
//   Both append moves to the provided MoveList.
// =============================================================================

void generateMoves(const Board& board, MoveList& list);
void generateCaptures(const Board& board, MoveList& list);

// =============================================================================
// INDIVIDUAL PIECE GENERATORS  (called internally by generateMoves)
//
//   Separated so we can test each piece type independently.
// =============================================================================

void generatePawnMoves  (const Board& board, MoveList& list);
void generateKnightMoves(const Board& board, MoveList& list);
void generateBishopMoves(const Board& board, MoveList& list);
void generateRookMoves  (const Board& board, MoveList& list);
void generateQueenMoves (const Board& board, MoveList& list);
void generateKingMoves  (const Board& board, MoveList& list);
