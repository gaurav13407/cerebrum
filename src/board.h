#pragma once
#include <cstdint>   // uint64_t, uint8_t
#include <string>    // std::string (for FEN parsing later)

// =============================================================================
// SQUARE INDEXING  (Little-Endian Rank-File / LERF)
//
//   a8=56  b8=57  ...  h8=63
//   a7=48  b7=49  ...  h7=55
//   ...
//   a1=0   b1=1   ...  h1=7
//
//   Each square is just an int 0..63.
//   Bit N of a bitboard = square N.
// =============================================================================

// Total squares on the board
constexpr int NUM_SQUARES = 64;

// Named square constants — use these instead of raw numbers in your code
// so you never have to guess which int means which square.
enum Square : int {
    A1=0,  B1,  C1,  D1,  E1,  F1,  G1,  H1,   // rank 1 = bits 0..7
    A2,    B2,  C2,  D2,  E2,  F2,  G2,  H2,   // rank 2 = bits 8..15
    A3,    B3,  C3,  D3,  E3,  F3,  G3,  H3,
    A4,    B4,  C4,  D4,  E4,  F4,  G4,  H4,
    A5,    B5,  C5,  D5,  E5,  F5,  G5,  H5,
    A6,    B6,  C6,  D6,  E6,  F6,  G6,  H6,
    A7,    B7,  C7,  D7,  E7,  F7,  G7,  H7,
    A8,    B8,  C8,  D8,  E8,  F8,  G8,  H8,   // rank 8 = bits 56..63
    NO_SQUARE = -1                               // sentinel: "no square"
};

// =============================================================================
// COLORS
// =============================================================================

enum Color : int {
    WHITE = 0,
    BLACK = 1,
    NUM_COLORS = 2
};

// Flip color: WHITE->BLACK, BLACK->WHITE
inline Color operator!(Color c) {
    return static_cast<Color>(c ^ 1);
}

// =============================================================================
// PIECE TYPES  (color-independent)
//
//   Used to index into per-color bitboard arrays.
//   e.g.  bitboards[WHITE][PAWN]  = all white pawns
// =============================================================================

enum PieceType : int {
    PAWN   = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK   = 3,
    QUEEN  = 4,
    KING   = 5,
    NUM_PIECE_TYPES = 6,
    NO_PIECE_TYPE   = 6   // sentinel
};

// =============================================================================
// PIECES  (color + type combined)
//
//   Used in the mailbox array — each of the 64 squares stores one of these.
//   Layout: 0=empty, 1..6=white pieces, 7..12=black pieces.
//
//   Formula:
//     piece = color * NUM_PIECE_TYPES + pieceType + 1
//   So:
//     WHITE_PAWN   = 0*6 + 0 + 1 = 1
//     WHITE_KING   = 0*6 + 5 + 1 = 6
//     BLACK_PAWN   = 1*6 + 0 + 1 = 7
//     BLACK_KING   = 1*6 + 5 + 1 = 12
// =============================================================================

enum Piece : int {
    EMPTY        = 0,
    WHITE_PAWN   = 1,
    WHITE_KNIGHT = 2,
    WHITE_BISHOP = 3,
    WHITE_ROOK   = 4,
    WHITE_QUEEN  = 5,
    WHITE_KING   = 6,
    BLACK_PAWN   = 7,
    BLACK_KNIGHT = 8,
    BLACK_BISHOP = 9,
    BLACK_ROOK   = 10,
    BLACK_QUEEN  = 11,
    BLACK_KING   = 12,
    NUM_PIECES   = 13
};

// =============================================================================
// HELPER FUNCTIONS: decode a Piece into Color / PieceType
// =============================================================================

// Returns WHITE or BLACK for a given piece.
// Undefined if piece == EMPTY — always check first.
inline Color colorOf(Piece p) {
    return static_cast<Color>((p - 1) / NUM_PIECE_TYPES);
}

// Returns PAWN..KING for a given piece.
// Undefined if piece == EMPTY.
inline PieceType typeOf(Piece p) {
    return static_cast<PieceType>((p - 1) % NUM_PIECE_TYPES);
}

// Build a Piece from color + type.
inline Piece makePiece(Color c, PieceType pt) {
    return static_cast<Piece>(c * NUM_PIECE_TYPES + pt + 1);
}

// =============================================================================
// CASTLING RIGHTS
//
//   Stored as 4 bits inside a uint8_t.
//   Each bit = one castling right still available.
// =============================================================================

enum CastlingRight : uint8_t {
    NO_CASTLING      = 0,
    WHITE_OO         = 1 << 0,   // bit 0: white kingside  (O-O)
    WHITE_OOO        = 1 << 1,   // bit 1: white queenside (O-O-O)
    BLACK_OO         = 1 << 2,   // bit 2: black kingside
    BLACK_OOO        = 1 << 3,   // bit 3: black queenside
    ALL_CASTLING     = 0b1111    // all four rights set
};

// =============================================================================
// BITBOARD HELPERS
//
//   A bitboard is just a uint64_t.
//   Bit N is set  ⟺  square N is "occupied" (by whatever the board tracks).
// =============================================================================

using Bitboard = uint64_t;

// Set bit N (mark square N as occupied)
inline void setBit(Bitboard& bb, int sq) {
    bb |= (1ULL << sq);
}

// Clear bit N (mark square N as empty)
inline void clearBit(Bitboard& bb, int sq) {
    bb &= ~(1ULL << sq);
}

// Test bit N (is square N occupied?)
inline bool testBit(Bitboard bb, int sq) {
    return (bb >> sq) & 1ULL;
}

// Pop the lowest set bit and return its index.
// Use this to iterate over all set bits:
//   while (bb) { int sq = popLSB(bb); ... }
inline int popLSB(Bitboard& bb) {
    int sq = __builtin_ctzll(bb);   // count trailing zeros = index of lowest bit
    bb &= bb - 1;                   // clear that bit
    return sq;
}

// Count how many bits are set (= how many squares are marked)
inline int popcount(Bitboard bb) {
    return __builtin_popcountll(bb);
}

// =============================================================================
// THE BOARD STRUCT
//
//   This is the full hybrid representation.
//   Two redundant pictures of the same position — always kept in sync.
// =============================================================================

struct Board {

    // -------------------------------------------------------------------------
    // PIECE-CENTRIC: bitboards[color][pieceType]
    //
    //   bitboards[WHITE][PAWN]   — one bit per white pawn still on the board
    //   bitboards[BLACK][QUEEN]  — one bit per black queen still on the board
    //   etc.
    //
    //   12 bitboards total (2 colors × 6 piece types).
    // -------------------------------------------------------------------------
    Bitboard bitboards[NUM_COLORS][NUM_PIECE_TYPES];

    // Derived occupancy bitboards — union of all pieces for one side, or both.
    // Kept in sync manually; used constantly in move generation.
    Bitboard occupancy[NUM_COLORS];   // occupancy[WHITE] = all white pieces
    Bitboard occupancyAll;            // all pieces on the board

    // -------------------------------------------------------------------------
    // SQUARE-CENTRIC: mailbox[square]
    //
    //   mailbox[E1] = WHITE_KING   (at game start)
    //   mailbox[E4] = EMPTY        (at game start)
    //
    //   64-element array, one entry per square.
    //   Each entry is a Piece enum value (EMPTY or a colored piece).
    // -------------------------------------------------------------------------
    Piece mailbox[NUM_SQUARES];

    // -------------------------------------------------------------------------
    // GAME STATE
    // -------------------------------------------------------------------------
    Color   sideToMove;        // whose turn it is right now
    uint8_t castlingRights;    // bitmask of CastlingRight flags still available
    int     enPassantSquare;   // target square for en passant, or NO_SQUARE
    int     halfMoveClock;     // moves since last pawn push or capture (50-move rule)
    int     fullMoveNumber;    // starts at 1, increments after black moves

    // -------------------------------------------------------------------------
    // CONSTRUCTOR & METHODS (implemented in board.cpp)
    // -------------------------------------------------------------------------

    // Default constructor: sets up the standard starting position
    Board();

    // Load a position from a FEN string (we'll implement this after move gen)
    // e.g. "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    void loadFEN(const std::string& fen);

    // Place a piece on a square — updates BOTH representations + occupancy.
    // Call this whenever you add a piece to the board.
    void putPiece(Piece p, Square sq);

    // Remove whatever piece is on a square — updates BOTH representations.
    // Call this before moving or capturing.
    void removePiece(Square sq);

    // Move a piece from one square to another (no capture).
    // Combines removePiece + putPiece in one call.
    void movePiece(Square from, Square to);

    // Print the board to stdout in ASCII for debugging
    void print() const;
};;
