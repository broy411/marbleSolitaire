# Marble Solitaire Solver

A high-performance C++ implementation of Marble (Peg) Solitaire with a full DFS-based solver, symmetry reduction, and bit-level board representation.

This project allows:

- Interactive play on an ASCII board
- Validation and execution of moves
- Automatic solving of arbitrary board states (when a solution exists)
- Canonicalization of board states via rotations and reflections to aggressively prune the search space

## Overview:

The project is split into three major components:

## msBoard:
  - Responsible for all board and move semantics:
  - Board representation (bit-level)
  - Move generation and validation
  - Applying and undoing moves
  - Symmetry transforms (rotations and reflections)
  - Canonical board computation
  - Win detection
All knowledge of how a board works or how a move is represented lives here.

## msSolver:

  - Responsible for solving a given board:
  - Depth-first search with backtracking
  - Canonicalization-based pruning
  - Optional memory-heavy bitmap or hash-set based “seen” tracking
  - Returns the full solution as a sequence of msBoard::Move
The solver is stateless from the caller’s perspective — all internal data structures are reset per solve.

## msGame:
  - Responsible for game orchestration and user interaction
  - Maintains the current board
  - Tracks move history
  - Allows undoing moves
  - Queries the solver for the best move or full solution
  - Acts as the high-level API that a UI or CLI would talk to

### Board Representation
  - Boards are represented as a uint64_t
  - The board is conceptually a 7×7 grid
  - Only 36 positions are playable
  - Non-playable positions are always zero
A bit value of:
  1 → marble present
  0 → empty
This compact representation enables:
  - Fast move application with bitwise ops
  - Extremely cheap copying
  - Efficient hashing and canonicalization

### Moves

A move is represented internally as:

  struct Move {
      Board setBit;     // Destination (becomes MARBLE)
      Board clearBits;  // Origin + jumped marble (become EMPTY)
  };

Key design decisions:
  - Move is owned by msBoard
  - Other modules may use moves but cannot construct arbitrary ones
     - This prevents invalid or inconsistent moves from being created
  
### Symmetry & Canonicalization
  - Each board has up to 8 equivalent states:
    - 4 rotations (0°, 90°, 180°, 270°)
    - 4 reflections (horizontal, vertical, diagonal, anti-diagonal)
  - Before a board is inserted into the solver’s “seen” structure:
    - All symmetric variants are generated
    - The lexicographically smallest bit pattern is selected
  - That canonical form is used for pruning
  - This reduces the search space dramatically and is the single biggest performance win.

### Solver Strategy
  - Depth-first search using an explicit stack
  - Shared move buffer to reduce allocations
  - Canonical pruning at every node

Optional backends for visited-state tracking:
  - Bitmap (if sufficient RAM is available)
  - Robin-Hood hash set fallback

The solver returns:
  - A vector of moves representing a valid solution
  - Or an empty vector if unsolvable

### Configuration
  - Some behavior is controlled at compile time via configuration.h, for example:
    - Whether to use a bitmap or hash set for visited boards
    - Memory-heavy optimizations when large RAM is available

### Error Handling & CREs

Throughout the codebase, you’ll see references to CRE.
  - CRE stands for Checked Runtime Error
  - In this project, a CRE simply means an assert(...) is used to enforce invariants.

CREs are used to:
  - Catch programmer errors
  - Enforce assumptions about board validity
  - Avoid defensive runtime checks in hot paths
  - They are not user-facing errors.

### Building

This project requires C++17 or newer.

Example build (clang): clang++ -std=c++17 -O2 -Wall -Wextra *.cpp -o msGame

### Performance Notes and Future Improvements

When running with O3, the average solve breaks down into the following time consumers:
  - getCanonicalBits()  = 29%
  - unordered_set usage = 28%
  - validMoves()        = 23%
  All profiling was done using Apple Instruments

Solve times range from less than a second on some starting boards to up to 10 minutes on unsolvable boards
  - From my own testing, the typical solve after a few moves have been made takes a couple seconds

Further gains are possible via:
  - Localized move generation -> change validMoves to only look at moves enabled by the previous move instead of all moves (downside is this changes the move indexing part of dfs algorithm)
  - Better move ordering heuristics -> sort ALL_MOVES based on how "forcing" a move is. Typically, moves toward the center are better, so this theoretically reduces sovle time on solvable boards.
  - Smaller canonical encodings -> there are less than 2^37 possible boards (symmetries), but I couldn't figure out a way to represent a canonical board in less than 37 bits.
                                    Accomplishing this would mean significantly less RAM storage and smaller initialization time for the bitmap




