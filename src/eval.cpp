#include "eval.h"
#include <cstring>

// =============================================================================
// PIECE SQUARE TABLE LOOKUP
//
//   Tables are written from white's perspective, rank 8 first.
//   Array index 0 = a8 in the table layout above.
//
//   To convert:
//     White: mirror the rank  → tableIndex = (7 - rank) * 8 + file
//     Black: use rank as-is   → tableIndex = rank * 8 + file
//
//   This way both sides get correct bonuses for "good" squares.
// =============================================================================

static inline int pstLookup(const int* table, int sq, Color c) {
    int rank = sq / 8;
    int file = sq % 8;
    int idx  = (c == WHITE) ? (7 - rank) * 8 + file   // mirror for white
                             : rank * 8 + file;         // normal for black
    return table[idx];
}

// Returns the PST bonus for a piece on a given square
static int pstBonus(PieceType pt, int sq, Color c, bool endgame) {
    switch (pt) {
        case PAWN:   return endgame ? pstLookup(PawnTableEG,   sq, c)
                                    : pstLookup(PawnTableMG,   sq, c);
        case KNIGHT: return endgame ? pstLookup(KnightTableEG, sq, c)
                                    : pstLookup(KnightTableMG, sq, c);
        case BISHOP: return endgame ? pstLookup(BishopTableEG, sq, c)
                                    : pstLookup(BishopTableMG, sq, c);
        case ROOK:   return endgame ? pstLookup(RookTableEG,   sq, c)
                                    : pstLookup(RookTableMG,   sq, c);
        case QUEEN:  return endgame ? pstLookup(QueenTableEG,  sq, c)
                                    : pstLookup(QueenTableMG,  sq, c);
        case KING:   return endgame ? pstLookup(KingTableEG,   sq, c)
                                    : pstLookup(KingTableMG,   sq, c);
        default:     return 0;
    }
}

// =============================================================================
// GAME PHASE CALCULATION  (tapered eval)
//
//   We calculate how far into the endgame we are.
//   Full material = middlegame (phase = 0).
//   No material   = endgame    (phase = 1.0).
//
//   Each piece type contributes to the phase score:
//     Queen=4, Rook=2, Bishop=1, Knight=1
//   Max total = 2*(4+2+2+1+1) = 24 for both sides.
//
//   We then interpolate:
//     score = (mgScore * phase + egScore * (24 - phase)) / 24
// =============================================================================

static int gamePhase(const Board& board) {
    int phase = 0;
    phase += popcount(board.bitboards[WHITE][QUEEN]  | board.bitboards[BLACK][QUEEN])  * 4;
    phase += popcount(board.bitboards[WHITE][ROOK]   | board.bitboards[BLACK][ROOK])   * 2;
    phase += popcount(board.bitboards[WHITE][BISHOP] | board.bitboards[BLACK][BISHOP]) * 1;
    phase += popcount(board.bitboards[WHITE][KNIGHT] | board.bitboards[BLACK][KNIGHT]) * 1;
    return phase;  // 0 = pure endgame, 24 = pure middlegame
}

// =============================================================================
// MATERIAL + PIECE SQUARE TABLES
//
//   Loop over all pieces for both sides.
//   Accumulate: piece value + PST bonus.
//   Return white_total - black_total.
// =============================================================================

static void evalMaterialAndPST(const Board& board, int phase,
                                int& mgScore, int& egScore) {
    for (int c = 0; c < NUM_COLORS; c++) {
        int sign = (c == WHITE) ? 1 : -1;

        for (int pt = 0; pt < NUM_PIECE_TYPES; pt++) {
            Bitboard bb = board.bitboards[c][pt];

            while (bb) {
                int sq = popLSB(bb);

                // Material value
                mgScore += sign * PieceValueMG[pt];
                egScore += sign * PieceValueEG[pt];

                // Positional bonus
                mgScore += sign * pstBonus(static_cast<PieceType>(pt), sq,
                                           static_cast<Color>(c), false);
                egScore += sign * pstBonus(static_cast<PieceType>(pt), sq,
                                           static_cast<Color>(c), true);
            }
        }
    }
}

// =============================================================================
// PAWN STRUCTURE EVALUATION
//
//   For each side:
//     - Doubled pawns:  multiple pawns on the same file → penalty
//     - Isolated pawns: no friendly pawns on adjacent files → penalty
//     - Passed pawns:   no enemy pawns blocking on same or adjacent files → bonus
// =============================================================================

// Precomputed masks — built inline here for simplicity
// fileMask[f]       = all squares on file f
// adjacentFiles[f]  = all squares on files f-1 and f+1

static Bitboard fileMask(int f) {
    Bitboard b = 0;
    for (int r = 0; r < 8; r++) b |= (1ULL << (r * 8 + f));
    return b;
}

static Bitboard adjacentFileMask(int f) {
    Bitboard b = 0;
    if (f > 0) b |= fileMask(f - 1);
    if (f < 7) b |= fileMask(f + 1);
    return b;
}

// Squares in front of a pawn (all ranks ahead of it for the given color)
static Bitboard frontSpan(int sq, Color c) {
    Bitboard b = 0;
    int rank = sq / 8;
    int file = sq % 8;
    if (c == WHITE) {
        for (int r = rank + 1; r < 8; r++) b |= (1ULL << (r * 8 + file));
    } else {
        for (int r = rank - 1; r >= 0; r--) b |= (1ULL << (r * 8 + file));
    }
    return b;
}

static void evalPawnStructure(const Board& board, int& mgScore, int& egScore) {
    for (int c = 0; c < NUM_COLORS; c++) {
        int sign  = (c == WHITE) ? 1 : -1;
        Color opp = (c == WHITE) ? BLACK : WHITE;

        Bitboard myPawns    = board.bitboards[c][PAWN];
        Bitboard theirPawns = board.bitboards[opp][PAWN];
        Bitboard bb         = myPawns;

        while (bb) {
            int sq   = popLSB(bb);
            int file = sq % 8;
            int rank = sq / 8;

            // ---- DOUBLED PAWNS ----
            // More than one of our pawns on this file?
            Bitboard sameFile = myPawns & fileMask(file);
            if (popcount(sameFile) > 1) {
                mgScore += sign * DOUBLED_PAWN_PENALTY;
                egScore += sign * DOUBLED_PAWN_PENALTY;
            }

            // ---- ISOLATED PAWNS ----
            // No friendly pawns on adjacent files?
            if (!(myPawns & adjacentFileMask(file))) {
                mgScore += sign * ISOLATED_PAWN_PENALTY;
                egScore += sign * ISOLATED_PAWN_PENALTY;
            }

            // ---- PASSED PAWNS ----
            // No enemy pawns on same file or adjacent files ahead of us?
            Bitboard passedMask = frontSpan(sq, static_cast<Color>(c))
                                | (frontSpan(sq, static_cast<Color>(c)) << 1 & ~fileMask(0))
                                | (frontSpan(sq, static_cast<Color>(c)) >> 1 & ~fileMask(7));

            // Simpler: check if any enemy pawn blocks on same/adjacent files ahead
            Bitboard blockZone = frontSpan(sq, static_cast<Color>(c));
            Bitboard adjAhead  = (blockZone << 1 & ~fileMask(0))
                               | (blockZone >> 1 & ~fileMask(7));
            (void)passedMask; // suppress unused warning

            if (!((blockZone | adjAhead) & theirPawns)) {
                // It's a passed pawn — bonus depends on how advanced it is
                int advRank = (c == WHITE) ? rank : (7 - rank);
                egScore += sign * PASSED_PAWN_BONUS[advRank];
                mgScore += sign * (PASSED_PAWN_BONUS[advRank] / 2);
            }
        }
    }
}

// =============================================================================
// KING SAFETY
//
//   Middlegame: check for pawn shield in front of the king.
//   3 pawns directly in front = maximum safety.
//   Each missing pawn = penalty.
// =============================================================================

static void evalKingSafety(const Board& board, int phase, int& mgScore, int& egScore) {
    // Only matters in the middlegame — scale by phase
    if (phase < 8) return;  // skip in endgame

    for (int c = 0; c < NUM_COLORS; c++) {
        int sign = (c == WHITE) ? 1 : -1;

        int kingSq   = __builtin_ctzll(board.bitboards[c][KING]);
        int kingFile = kingSq % 8;
        int kingRank = kingSq / 8;

        Bitboard myPawns = board.bitboards[c][PAWN];

        // Check the 3 squares directly in front of the king
        int shieldRank = (c == WHITE) ? kingRank + 1 : kingRank - 1;
        if (shieldRank >= 0 && shieldRank < 8) {
            for (int df = -1; df <= 1; df++) {
                int f = kingFile + df;
                if (f < 0 || f > 7) continue;
                int shieldSq = shieldRank * 8 + f;
                if (myPawns & (1ULL << shieldSq))
                    mgScore += sign * PAWN_SHIELD_BONUS;
                else
                    mgScore -= sign * PAWN_SHIELD_BONUS;  // missing shield = penalty
            }
        }
    }
}

// =============================================================================
// BISHOP PAIR BONUS
//
//   Having both bishops is worth ~30-50 cp in open positions.
//   The advantage grows in the endgame.
// =============================================================================

static void evalBishopPair(const Board& board, int& mgScore, int& egScore) {
    for (int c = 0; c < NUM_COLORS; c++) {
        int sign = (c == WHITE) ? 1 : -1;
        if (popcount(board.bitboards[c][BISHOP]) >= 2) {
            mgScore += sign * 30;
            egScore += sign * 50;
        }
    }
}

// =============================================================================
// ROOK ON OPEN FILE BONUS
//
//   A rook on a file with no friendly pawns = semi-open file (+10).
//   A rook on a file with no pawns at all   = open file (+20).
// =============================================================================

static void evalRookFiles(const Board& board, int& mgScore, int& egScore) {
    for (int c = 0; c < NUM_COLORS; c++) {
        int sign  = (c == WHITE) ? 1 : -1;
        Color opp = (c == WHITE) ? BLACK : WHITE;

        Bitboard rooks      = board.bitboards[c][ROOK];
        Bitboard myPawns    = board.bitboards[c][PAWN];
        Bitboard theirPawns = board.bitboards[opp][PAWN];

        while (rooks) {
            int sq   = popLSB(rooks);
            int file = sq % 8;
            Bitboard fm = fileMask(file);

            bool noMyPawn    = !(myPawns    & fm);
            bool noTheirPawn = !(theirPawns & fm);

            if (noMyPawn && noTheirPawn) {
                mgScore += sign * 20;   // open file
                egScore += sign * 20;
            } else if (noMyPawn) {
                mgScore += sign * 10;   // semi-open file
                egScore += sign * 10;
            }
        }
    }
}

// =============================================================================
// MAIN EVALUATE FUNCTION
// =============================================================================

int evaluate(const Board& board) {
    int mgScore = 0;
    int egScore = 0;

    // Calculate game phase (24 = full middlegame, 0 = pure endgame)
    int phase = gamePhase(board);

    // Accumulate all terms into mg and eg scores separately
    evalMaterialAndPST(board, phase, mgScore, egScore);
    evalPawnStructure (board,        mgScore, egScore);
    evalKingSafety    (board, phase, mgScore, egScore);
    evalBishopPair    (board,        mgScore, egScore);
    evalRookFiles     (board,        mgScore, egScore);

    // Tapered eval: interpolate between middlegame and endgame scores
    // phase=24 → pure mg, phase=0 → pure eg
    int score = (mgScore * phase + egScore * (24 - phase)) / 24;

    // Return from the perspective of the side to move
    // Positive = "I'm winning", negative = "I'm losing"
    return (board.sideToMove == WHITE) ? score : -score;
}
