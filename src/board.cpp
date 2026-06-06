#include "board.h"
#include <iostream>
#include <cassert>

// putPiece 
// places piece p on sqaure sq 
// precondition:sq must currently  be empty
// Always remove peice first if a piece is already there
void Board::putPiece(Piece p, Square sq){
    assert(p !=EMPTY); //cant place empty 
    assert(mailbox[sq] == EMPTY); // square must be free 

    Color c =colorOf(p);
    PieceType pt=typeOf(p);

    setBit(bitboards[c][pt], sq); //mark this square in piece bitboards
    setBit (occupancy[c],sq); //mark the sqaure in color coccpancy
    setBit(occupancyAll,sq); //write into mailbox
    mailbox[sq]=p;
}

//remove Piece 
//remove whatever peice in on squre square
//update all four data sturcutre accordinly

void Board::removePiece(Square sq){
    Piece p=mailbox[sq];
    if(p==EMPTY) return;

    Color  c=colorOf(p);
    PieceType pt=typeOf(p);

    clearBit(bitboards[c][pt],sq);
    clearBit(occupancy[c],sq);
    clearBit(occupancyAll, sq);
    mailbox[sq]=EMPTY;

}

// move piece 
// move piece from to 'to' with no capture 
// 'to' must be empty-callremovePiece(to) first for captures


void Board::movePiece(Square from,Square to){
    Piece p=mailbox[from];
    assert(p!=EMPTY);
    assert(mailbox[to]==EMPTY);

    Color c=colorOf(p);
    PieceType pt=typeOf(p);

    Bitboard mask=(1ULL << from)| (1ULL << to);
    bitboards[c][pt]^=mask;
    occupancy[c] ^=mask;
    occupancyAll ^=mask;

    //update the mailbox
    mailbox[to]=p;
    mailbox[from]=EMPTY;
}


// Board constuctor-sets up the standard chess starting postion 


 
Board::Board() {
    // --- Zero everything out first ---
 
    for (int c = 0; c < NUM_COLORS; c++)
        for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
            bitboards[c][pt] = 0ULL;
 
    occupancy[WHITE] = occupancy[BLACK] = occupancyAll = 0ULL;
 
    for (int sq = 0; sq < NUM_SQUARES; sq++)
        mailbox[sq] = EMPTY;
 
    // --- Game state ---
    sideToMove      = WHITE;
    castlingRights  = ALL_CASTLING;     // K Q k q all available at start
    enPassantSquare = NO_SQUARE;        // no en passant possible at start
    halfMoveClock   = 0;
    fullMoveNumber  = 1;
 
    // --- Place white pieces ---
    // Rank 1 (back rank): R N B Q K B N R
    putPiece(WHITE_ROOK,   A1);
    putPiece(WHITE_KNIGHT, B1);
    putPiece(WHITE_BISHOP, C1);
    putPiece(WHITE_QUEEN,  D1);
    putPiece(WHITE_KING,   E1);
    putPiece(WHITE_BISHOP, F1);
    putPiece(WHITE_KNIGHT, G1);
    putPiece(WHITE_ROOK,   H1);
 
    // Rank 2 (white pawns): all 8 squares A2..H2
    for (int file = 0; file < 8; file++)
        putPiece(WHITE_PAWN, static_cast<Square>(A2 + file));
 
    // --- Place black pieces ---
    // Rank 8 (back rank): r n b q k b n r
    putPiece(BLACK_ROOK,   A8);
    putPiece(BLACK_KNIGHT, B8);
    putPiece(BLACK_BISHOP, C8);
    putPiece(BLACK_QUEEN,  D8);
    putPiece(BLACK_KING,   E8);
    putPiece(BLACK_BISHOP, F8);
    putPiece(BLACK_KNIGHT, G8);
    putPiece(BLACK_ROOK,   H8);
 
    // Rank 7 (black pawns): all 8 squares A7..H7
    for (int file = 0; file < 8; file++)
        putPiece(BLACK_PAWN, static_cast<Square>(A7 + file));
}
 
// =============================================================================
// print
//
//   Prints the board to stdout in ASCII for debugging.
//   Uppercase = white, lowercase = black, '.' = empty.
//
//   Output looks like:
//     8 | r n b q k b n r
//     7 | p p p p p p p p
//     6 | . . . . . . . .
//     ...
//     1 | R N B Q K B N R
//         a b c d e f g h
// =============================================================================
 
void Board::print() const {
    // Symbol for each Piece enum value
    const char* symbols[NUM_PIECES] = {
        ".",   // EMPTY
        "P", "N", "B", "R", "Q", "K",   // white pieces
        "p", "n", "b", "r", "q", "k"    // black pieces
    };
 
    // Print from rank 8 down to rank 1 (top of board to bottom)
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;               // LERF: sq = rank*8 + file
            std::cout << symbols[mailbox[sq]] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "    a b c d e f g h\n\n";
 
    // Print side to move
    std::cout << "Side to move : " << (sideToMove == WHITE ? "White" : "Black") << "\n";
 
    // Print castling rights
    std::cout << "Castling     : ";
    if (castlingRights & WHITE_OO)  std::cout << "K";
    if (castlingRights & WHITE_OOO) std::cout << "Q";
    if (castlingRights & BLACK_OO)  std::cout << "k";
    if (castlingRights & BLACK_OOO) std::cout << "q";
    if (castlingRights == NO_CASTLING) std::cout << "-";
    std::cout << "\n";
 
    // Print en passant
    std::cout << "En passant   : ";
    if (enPassantSquare == NO_SQUARE) {
        std::cout << "-\n";
    } else {
        // Convert square index back to algebraic notation (e.g. 16 -> "a3")
        char file = 'a' + (enPassantSquare % 8);
        char rank = '1' + (enPassantSquare / 8);
        std::cout << file << rank << "\n";
    }
 
    std::cout << "Half moves   : " << halfMoveClock  << "\n";
    std::cout << "Full moves   : " << fullMoveNumber  << "\n";
}

