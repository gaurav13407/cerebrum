# ♟️ Cerebrum Chess Engine

A UCI-compatible chess engine written in C++ from scratch.  
Uses bitboard board representation, alpha-beta pruning with iterative  
deepening, and a hand-tuned evaluation function.

---

## Features

- **Bitboard representation** — 64-bit integers for fast move generation
- **Legal move generation** — all piece types including castling, en passant, promotions
- **Alpha-Beta search** with iterative deepening
- **Transposition table** using Zobrist hashing
- **Move ordering** — MVV-LVA, killer moves, history heuristic
- **Quiescence search** — avoids horizon effect on captures
- **Null move pruning** and **Late Move Reductions (LMR)**
- **Evaluation** — material, piece-square tables, pawn structure, king safety
- **UCI protocol** — works with Arena, CuteChess, Lichess bot

---

## Build

```bash
git clone https://github.com/yourusername/cerebrum
cd cerebrum
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage

Run with any UCI-compatible GUI (Arena, CuteChess):

```bash
./cerebrum
```

Or use via UCI commands directly:

uci
isready
position startpos moves e2e4 e7e5
go depth 10

---

## Architecture
src/
├── board.h / board.cpp       # Board state, Zobrist hashing
├── movegen.h / movegen.cpp   # Legal move generation (bitboards)
├── search.h / search.cpp     # Alpha-beta, iterative deepening
├── eval.h / eval.cpp         # Position evaluation
├── tt.h / tt.cpp             # Transposition table
├── uci.h / uci.cpp           # UCI protocol handler
└── main.cpp                  # Entry point

---

## Roadmap

- [x] Board representation (bitboards)
- [x] Move generation
- [x] Alpha-beta search
- [x] Transposition table
- [x] UCI protocol
- [ ] NNUE evaluation
- [ ] Endgame tablebases (Syzygy)
- [ ] Lichess bot deployment

---

## Resources

- [Chess Programming Wiki](https://www.chessprogramming.org)
- [Stockfish source](https://github.com/official-stockfish/Stockfish)
- [Sebastian Lague's Chess series](https://www.youtube.com/c/SebastianLague)

---

## License

MIT
