#pragma once
#include "board.h"

// =============================================================================
// SCORE CONSTANTS
//
//   All scores are in centipawns (cp).
//   1 pawn = 100 cp. This gives fine-grained scoring.
//
//   INF     = score we use for "checkmate / no bound"
//   MATE    = base checkmate score (we subtract depth so
//             engine prefers faster mates)
// =============================================================================

constexpr int INF  = 1'000'000;
constexpr int MATE = 900'000;

// =============================================================================
// PIECE VALUES  (middlegame and endgame separately)
//
//   Pieces are worth slightly different amounts depending on game phase.
//   Rooks and queens gain value in the endgame (open board, active).
//   Knights lose value (fewer pieces to maneuver around).
// =============================================================================

constexpr int PieceValueMG[NUM_PIECE_TYPES] = {
//  PAWN  KNIGHT  BISHOP  ROOK  QUEEN  KING
    100,   320,    330,   500,   900,    0
};

constexpr int PieceValueEG[NUM_PIECE_TYPES] = {
//  PAWN  KNIGHT  BISHOP  ROOK  QUEEN  KING
    120,   290,    310,   550,   950,    0
};

// =============================================================================
// PIECE-SQUARE TABLES  (middlegame)
//
//   Each table has 64 entries — one per square.
//   Positive = good square for this piece, negative = bad square.
//   Tables are from WHITE's perspective (rank 1 at the bottom).
//   For black we mirror vertically (flip the rank index).
//
//   Layout: index 0 = a1, index 7 = h1, index 56 = a8, index 63 = h8
//   Reading order in the array below: rank 8 first (top), rank 1 last (bottom)
//   so it visually matches a chess board.
// =============================================================================

// PAWN — reward advancement and center control
constexpr int PawnTableMG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,   // rank 8 (promotion rank, pawns never here)
    50, 50, 50, 50, 50, 50, 50, 50,   // rank 7 (one step from promotion)
    10, 10, 20, 30, 30, 20, 10, 10,   // rank 6
     5,  5, 10, 25, 25, 10,  5,  5,   // rank 5
     0,  0,  0, 20, 20,  0,  0,  0,   // rank 4
     5, -5,-10,  0,  0,-10, -5,  5,   // rank 3
     5, 10, 10,-20,-20, 10, 10,  5,   // rank 2 (starting rank)
     0,  0,  0,  0,  0,  0,  0,  0    // rank 1 (never here)
};

// PAWN endgame — passed pawn advancement matters more
constexpr int PawnTableEG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    80, 80, 80, 80, 80, 80, 80, 80,
    50, 50, 50, 50, 50, 50, 50, 50,
    30, 30, 30, 30, 30, 30, 30, 30,
    20, 20, 20, 20, 20, 20, 20, 20,
    10, 10, 10, 10, 10, 10, 10, 10,
     5,  5,  5,  5,  5,  5,  5,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

// KNIGHT — centralization is everything for knights
constexpr int KnightTableMG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr int KnightTableEG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

// BISHOP — reward long diagonals and avoiding corners
constexpr int BishopTableMG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr int BishopTableEG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

// ROOK — reward open files and 7th rank
constexpr int RookTableMG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

constexpr int RookTableEG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

// QUEEN — slight centralization, don't develop too early
constexpr int QueenTableMG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr int QueenTableEG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

// KING middlegame — hide behind pawns, prefer castled position
constexpr int KingTableMG[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

// KING endgame — be active, go to the center
constexpr int KingTableEG[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

// =============================================================================
// PAWN STRUCTURE PENALTIES
// =============================================================================

constexpr int DOUBLED_PAWN_PENALTY  = -20;  // per doubled pawn
constexpr int ISOLATED_PAWN_PENALTY = -30;  // per isolated pawn
constexpr int PASSED_PAWN_BONUS[8]  = {     // bonus by rank (rank 0..7)
    0, 10, 20, 30, 60, 100, 150, 0
};

// =============================================================================
// KING SAFETY
// =============================================================================

constexpr int PAWN_SHIELD_BONUS = 10;   // per pawn directly shielding the king

// =============================================================================
// MAIN EVALUATION FUNCTION
// =============================================================================

// Returns score in centipawns from the side-to-move's perspective.
// Positive = I'm winning. Negative = I'm losing.
int evaluate(const Board& board);
