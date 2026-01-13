/*
    msBoard.cpp
    January 4th, 2026
    Brendan Roy

    This file implements the msBoard.h interface. An msBoard is a board for
    French Marble Solitaire, where users can print the board, make moves, and
    generate valid moves. It's designed to be used by another class to
    implement Marble Solitaire.

*/

#include "configuration.h"
#include "msBoard.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#if HAVE_PEXT
    #include <immintrin.h>
#endif




/************************** structs and types: ****************************/
/*
    All boards are represented as a uint64_t. 
    Each board is structured the same way with the following invariants:
        - The most significant 49 bits represent the board, but only 36 of 
            those are valid positions where marbles can be
        - Bits corresponding to non-playable positions are always 0
        - Bits corresponding to playable positions are:
            1 = MARBLE
            0 = EMPTY
        - Therefore, a "winning" board has exactly 1 bit set (one marble)

        The default starting board looks like this (ignoring invalid spots):
                    011
                   11111
                  1111111
                  1111111
                  1111111
                   11111
                    111  
            
*/
using Board = uint64_t;

/* 
    Since boards are 7x7 squares (the valid positions do not form a square,
    but there are 7 rows and 7 columns), each row and column is consists of 
    7 bits. Here, we define each to be uint8_t, but by convention, we only 
    use the seven LSB to store information. The MSB (usually 0) is irrelevant
*/
using Row = uint8_t;
using Column = uint8_t;

/***** struct Move *****
 Contains all information needed to make a move on any given board
    Members:
    uint64_t setBit    - A bit mask of all 0s except one 1 at the bit index
                            of the position that will be a MARBLE after the 
                            move is executed
    uint64_t clearBits - A bit mask of all 0s except two 1s at the bit
                            indices of the positions that will be EMPTY after
                            the move is executed
******************/
// The struct Move definition is in the msBoard.h file

// ALL_MOVES stores every possible move to be made on the board
const std::vector<msBoard::Move> msBoard::ALL_MOVES = msBoard::setupAllMoves();

// How the board represents marbles (●) and empty positions (.)
#define TO_STRING(bit) ((bit) ? "●" : ".")


// Anonymous Namespace:
namespace {
    /*************************** Enum and Constants ***************************/

    constexpr int NUM_ROTATIONS = 8;

    constexpr int NUM_ROWS = 7;
    constexpr int NUM_COLS = 7;
    constexpr int MAX_ROW = NUM_ROWS - 1;
    constexpr int MAX_COL = NUM_COLS - 1;
    constexpr int MAX_BOARD_IDX = 63;


    const bool PLAYABLE[NUM_ROWS][NUM_COLS] = {
                                               {0,0,1,1,1,0,0},
                                               {0,1,1,1,1,1,0},
                                               {1,1,1,1,1,1,1},
                                               {1,1,1,1,1,1,1},
                                               {1,1,1,1,1,1,1},
                                               {0,1,1,1,1,1,0},
                                               {0,0,1,1,1,0,0}
                                              };
    #define ROW_IDX(r) (MAX_BOARD_IDX - ((r) * NUM_COLS))
    constexpr int rowShift[NUM_ROWS] = {
                                        ROW_IDX(0) - (NUM_COLS - 1),
                                        ROW_IDX(1) - (NUM_COLS - 1),
                                        ROW_IDX(2) - (NUM_COLS - 1),
                                        ROW_IDX(3) - (NUM_COLS - 1),
                                        ROW_IDX(4) - (NUM_COLS - 1),
                                        ROW_IDX(5) - (NUM_COLS - 1),
                                        ROW_IDX(6) - (NUM_COLS - 1)
                                       };

    const uint64_t DEFAULT_BOARD =    0x18FBFFFFEF8E0000; // - empty (0, 2) .14 fastest, .34rn
    // const Board DEFAULT_BOARD = 0x38FBFFFEEF8E0000; // - empty (2,3) 63s -> 56 -> 43 (w O3) -> 25 (w 16GiB) -> 25 (w REVERSED - others got faster)
    // const Board DEFAULT_BOARD =    0x38DBFFFFEF8E0000; // - empty (1, 3) 1.47s
    const Board FULL_BOARD =       0x38FBFFFFEF8E0000;
    const Board EMPTY_BOARD = 0ULL;

    
    const int WINNING_MARBLE_COUNT = 1;

    /* 
      COL_START_IDX[i] and COL_END_IDX[i] indicate at what indices of a 7x7 
      board the valid columns (where marbles can be) in row i begin and end. 
      Treating board as a 2d array, every  index board[i][j] is valid
      for 0 <= i < NUM_ROWS and COL_START_IDX[i] <= j <= COL_END_IDX[i].
                                     Note the <= sign ^
    */
    const int COL_START_IDX[] = {2,1,0,0,0,1,2};
    const int COL_END_IDX[]   = {4,5,6,6,6,5,4};
    

    std::array<uint8_t, 128> reverseAllRowCols();
    std::array<Board, NUM_COLS> getColMasks();

    const std::array<uint8_t, 128> REVERSED = reverseAllRowCols();
    const std::array<Board, NUM_COLS> COL_MASKS = getColMasks();

    /************************ Private Helper Functions ************************/

   /************ reverse7 *********
     Reverses the least significant 7 bits of an integer - can take in either
     a Row or a Column

    Parameters: 
        uint8_t x - an 8-bit integer that we reverse 
    Returns: 
        A uint8_t - MSB is 0, next 7 are the 7 LSB from x in reverse order
    Expects: 
        Nothing
    Notes:
        The MSB of x is ignored and does not affect the return value
    *********************************/
    inline uint8_t reverse7(uint8_t x) {
        uint8_t rev = 0;
        rev |= (x & 0x01) << 6; // bit0 -> bit6
        rev |= (x & 0x02) << 4; // bit1 -> bit5
        rev |= (x & 0x04) << 2; // bit2 -> bit4
        rev |= (x & 0x08);      // bit3 -> bit3
        rev |= (x & 0x10) >> 2; // bit4 -> bit2
        rev |= (x & 0x20) >> 4; // bit5 -> bit1
        rev |= (x & 0x40) >> 6; // bit6 -> bit0
        return rev;
    }

    std::array<uint8_t, 128> reverseAllRowCols()
    {
        std::array<uint8_t, 128> reversed;
        for (int i = 0; i < 128; i++) {
            reversed[i] = reverse7(i);
        }
        return reversed;
    }


    /************ bitIndex *********
     Returns the bit index of a given position on the board
    
    Parameters:
        unsigned row - the row of the position
        unsigned col - the column of the position
    Returns: 
        An unsigned integer that corresponds to the given position's coordinates 
        on a board - note that boards are indexed starting from the MSB
    Expects: 
        Both row and col must be less than 7
    Notes:
        Will CRE if row or col >= 7
    *********************************/
    inline unsigned bitIndex(unsigned r, unsigned c) {
        return ROW_IDX(r) - c;
    }


    /************ getRow *********
     Gets a row from a given board

    Parameters: 
        Board b    - the board that we retrieve the row from
        unsigned r - the row number that we get
    Returns: 
        A uint8_t containing the bits of the given row. The MSB is always 0,
        the next 7 are the bits of row number 2 in b, in left-to-right order
    Expects: 
        b should follow the Board invariants
        r must be a valid row number
    Notes:
        Will CRE if r >= NUM_ROWS
        Will return an invalid Board if given one - does not check the board's
        validity
    *********************************/
    inline Row getRow(Board b, unsigned r) {
        assert(r < NUM_ROWS);

        return (b >> rowShift[r]) & 0x7F;
    }


    /************ insertRow *********
     Puts a given row of bits into the given board at a given index. Returns a 
     new version of the board with the old row swapped for the new one. No other
     changes to the board are made.

    Parameters: 
        Board b         - the board that we update, passed by value
        unsigned r      - the row that we modify
        uint8_t rowBits - the new row that replaces the row number r in b
    Returns: 
        An updated version of b - row r of b is substituted with the 7 LSB of
        rowBits, ordered from most significant to least in the row
    Expects: 
        r must be a valid row number (r < NUM_ROWS)
    Notes:
        - Does not check for valid input - invalid input CAN return a Board
          that does not follow the invariants. Invalid input would consist of a
          row of all 1s with r = 0 --> violates void positions of board being 
          set to 0
        - The MSB of rowBits is ignored
        - Will CRE if r >= NUM_ROWS
    *********************************/
    inline Board insertRow(Board b, unsigned r, Row rowBits) {
        assert(r < NUM_ROWS);

        return (b & ~(Board(0x7F) << (rowShift[r]))) |
               (Board)rowBits << rowShift[r];
    }


    /************ getCol *********
     Get a specific column from a given board

    Parameters: 
        Board b    - the board that we retrieve from, passed by value
        unsigned c - the column index to retrieve
    Returns: 
        A uint8_t containing the bits of the given column. The MSB is always 0,
        the next 7 are the bits of board's c column, in top-to-bottom order 
    Expects: 
        b should follow Board's invariants
        c must be a valid column number (ie c < NUM_COLS)
    Notes:
        Will CRE if c >= NUM_COLS
        Will return an invalid Board if given one - does not check the board's
        validity
    *********************************/
    inline Column getCol(Board b, unsigned c) {
        assert(c < NUM_COLS);
       #if HAVE_PEXT
            return (Column)_pext_u64(b, COL_MASKS[c]);
       #else
           return ((b >> (ROW_IDX(0) - c)) & 1ULL) << 6 |
                  ((b >> (ROW_IDX(1) - c)) & 1ULL) << 5 |
                  ((b >> (ROW_IDX(2) - c)) & 1ULL) << 4 |
                  ((b >> (ROW_IDX(3) - c)) & 1ULL) << 3 |
                  ((b >> (ROW_IDX(4) - c)) & 1ULL) << 2 |
                  ((b >> (ROW_IDX(5) - c)) & 1ULL) << 1 |
                  ((b >> (ROW_IDX(6) - c)) & 1ULL) << 0;
       #endif
    }
    

    std::array<Board, NUM_COLS> getColMasks()
    {
        std::array<Board, NUM_COLS> masks;
        for (int c = 0; c < NUM_COLS; c++) {
            masks[c] = (1ULL << (ROW_IDX(0) - c)) |
                       (1ULL << (ROW_IDX(1) - c)) |
                       (1ULL << (ROW_IDX(2) - c)) |
                       (1ULL << (ROW_IDX(3) - c)) |
                       (1ULL << (ROW_IDX(4) - c)) |
                       (1ULL << (ROW_IDX(5) - c)) |
                       (1ULL << (ROW_IDX(6) - c));
        }
        return masks;
    }
 

    /************ getBit *********
     Gets a bit from a Board at a given index - returns the bit in the LSB of
     a uint64_t - tells us whether the board holds a marble at position (r, c)
    
    Parameters: 
        Board b    - the board that we get the bit from
        unsigned r - the row of the bit we get
        unsigned c - the column of the bit we get
    Returns: 
        uint64_t whose LSB holds the bit at (r, c) - ie either a 0ULL or 1ULL
    Expects: 
        r and c must be valid row and column indices
    Notes:
        Will CRE if r > NUM_ROWS or if c > NUM_COLS
    *********************************/
    inline uint64_t getBit(Board b, unsigned r, unsigned c) {
        assert(r < NUM_ROWS);
        assert(c < NUM_COLS);

        return (b >> bitIndex(r, c)) & 1ULL;
    }

     /************ transformBoard *********
     Takes in a Board and a Transform and returns a new Board with the
     transformation applied to it
    
    Parameters: 
        Board b     - the board to apply the Transformation to
        Transform t - the transformation to do
    Returns: 
        A new, transformed version of b
    *********************************/
    inline Board transformBoard(Board b, msBoard::Transform t) {
        Board out = EMPTY_BOARD;

        switch(t) {
            case msBoard::DEGREE_0: return b;
            case msBoard::DEGREE_90:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Column col = getCol(b,i);
                    out = insertRow(out, MAX_ROW - i, col);
                }
                break;
            case msBoard::DEGREE_180:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Row row = getRow(b,i);
                    out = insertRow(out, MAX_ROW - i, REVERSED[row]);
                }
                break;
            case msBoard::DEGREE_270:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Column col = getCol(b,i);
                    out = insertRow(out, i, REVERSED[col]);
                }
                break;
            case msBoard::FLIP_H:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Row row = getRow(b,i);
                    out = insertRow(out, i, REVERSED[row]);
                }
                break;
            case msBoard::FLIP_V:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Row row = getRow(b,i);
                    out = insertRow(out, MAX_ROW - i, row);
                }
                break;
            case msBoard::FLIP_DIAG:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Column col = getCol(b,i);
                    out = insertRow(out, i, col);
                }
                break;
            case msBoard::FLIP_ANTI:
                for (int i = 0; i < NUM_ROWS; i++) {
                    Column col = getCol(b,i);
                    out = insertRow(out, MAX_ROW - i, REVERSED[col]);
                }
                break;
        }
        return out;
    }

     /************ inverseTransform *********
     Takes in a Transform and returns the inverse of that Transform
    
    Parameters: 
        msBoard::Transform t - the transform to analyze
    Returns: 
        An msBoard::Transform - the inverse of the provided transform
    *********************************/ 
    inline msBoard::Transform inverseTransform(msBoard::Transform t) {
        switch(t){
            case msBoard::DEGREE_0:   return msBoard::DEGREE_0;
            case msBoard::DEGREE_90:  return msBoard::DEGREE_270;
            case msBoard::DEGREE_180: return msBoard::DEGREE_180;
            case msBoard::DEGREE_270: return msBoard::DEGREE_90;
            case msBoard::FLIP_H:     return msBoard::FLIP_H;
            case msBoard::FLIP_V:     return msBoard::FLIP_V;
            case msBoard::FLIP_DIAG:  return msBoard::FLIP_DIAG;
            case msBoard::FLIP_ANTI:  return msBoard::FLIP_ANTI;
        }
        return msBoard::DEGREE_0; // unreachable
    }

}
/*********************** Private Member Functions *****************************/
/************ setupAllMoves *********
 Generates all possible moves that can ever be played during a game

Parameters: none
Returns: 
    vector<Move> containing all the possible moves
Expects: 
    All constants must have their correct values
Notes:
    Will not throw any errors
*********************************/
std::vector<msBoard::Move> msBoard::setupAllMoves()  
{
    std::vector<msBoard::Move> moves;

    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
            if (!PLAYABLE[r][c]) continue;

            unsigned src = bitIndex(r, c);

            if (r >= 2 && PLAYABLE[r-1][c] && PLAYABLE[r-2][c])
                moves.push_back(Move(Board(1) << bitIndex(r-2, c),
                                     (Board(1) << src) | 
                                     (Board(1) << bitIndex(r-1, c))));

            if (r <= 4 && PLAYABLE[r+1][c] && PLAYABLE[r+2][c])
                moves.push_back(Move(Board(1) << bitIndex(r+2, c),
                                     (Board(1) << src) | 
                                     (Board(1) << bitIndex(r+1, c))));

            if (c >= 2 && PLAYABLE[r][c-1] && PLAYABLE[r][c-2])
                moves.push_back(Move(Board(1) << bitIndex(r, c-2),
                                     (Board(1) << src) | 
                                     (Board(1) << bitIndex(r, c-1))));

            if (c <= 4 && PLAYABLE[r][c+1] && PLAYABLE[r][c+2])
                moves.push_back(Move(Board(1) << bitIndex(r, c+2),
                                     (Board(1) << src) | 
                                     (Board(1) << bitIndex(r, c+1))));
        }
    }

    return moves;
}

/**************************** msBoard public functions ************************/


/************ msBoard - public default constructor *********
 Constructor for msBoard class - uses the DEFAULT board that has the open
 marble spot as the top left position (0, 2)

Parameters: none
Returns: 
    An instance of the msBoard class
*********************************/
msBoard::msBoard()
{
    board = DEFAULT_BOARD;
}
/************ msBoard - public custom constructor *********
 Constructor for msBoard class - users can choose which position of the board
 starts with an empty marble

Parameters:
    unsigned row - row of the position to start empty
    unsigned col - column of the position to start empty
Returns: 
    An instance of the msBoard class
Notes:
    Will use the DEFAULT board if the row or col is out of bounds
*********************************/
msBoard::msBoard(unsigned row, unsigned col) 
{
    if (row > MAX_ROW || col > MAX_COL || !PLAYABLE[row][col]) {
        board = DEFAULT_BOARD;
    } else {
        board = FULL_BOARD;
        Board mask = Board(1) << (bitIndex(row, col));
        board &= ~mask;
    }
}


/************ hasWon *********
 Checks whether a given board is in a winning position

Parameters:
    Board board - the board to be analyzed, passed by reference
Returns: 
    A bool - true if exactly one playable position contains a marble
Expects: 
    Nothing, but will only work as intended on a valid board
Notes:
    Will not throw any errors
*********************************/
bool msBoard::hasWon() const 
{
    return __builtin_popcountll(board) == WINNING_MARBLE_COUNT;
}

/************ validMoves *********
 Appends all possible valid moves on the given board to the given vector

Parameters:
    Board board - the board to be analyzed, passed by reference
    vector<const Move*> &moves - a reference to a vector of Move 
                                    pointers to be appended with all current
                                    valid moves on the given board
Returns: void
Expects: 
    board satisfies the Board invariants
Notes:
    Appends (does not clear) valid moves for this board to moves
*********************************/
void msBoard::validMoves(std::vector<msBoard::Move> &moves) const 
{
    const Board occupied = board;
    const Board empty = ~board;
    
    for (const Move& m : ALL_MOVES) {
        const bool valid = ((occupied & m.clearBits) == m.clearBits) & 
                          ((empty & m.setBit) == m.setBit);
        if (valid) moves.push_back(m);
    }
}

/************ applyMove *********
 Given a board and a move, it returns the same board but with the move 
    applied to it

Parameters:
    Board board - the board that we make a move on, passed by value
    const Move *m - a pointer to the Move that is executed on the board
Returns: 
    A new Board that is unchanged except for the applied move
Expects: 
    m is a valid move
Notes:

*********************************/
 msBoard msBoard::applyMove(const msBoard::Move m) const 
 {
    Board next = (board | m.setBit) & ~(m.clearBits);
    return msBoard(next);
}

/************ boardToBits *********
 Converts a board to its minimum representation of just 37 bits - each unique 
 board will correspond exactly one output of this function

Parameters: none
Returns: 
    A uint64_t - the least significant 37 bits contain a packed version of the
                 board - exact structure is not relevant
Notes:
    This is not designed for board representation but rather a way to map every
    possible board to a single representation of 37 bits
*********************************/
uint64_t msBoard::boardToBits() const 
{
        Board ret = EMPTY_BOARD;
        Board b = board;
        /*
          - Shift b by however many bits until end of row
          - Add however many bits that row has to MSB-side of ret
          - Repeat for each row
          This gives us all the bits of the playable boardspace
        */
        b >>= 17;
        ret |= (b & 0x7); // ret has 3 bits
        b >>= 6;
        ret |= (b & 0x1F) << 3; // ret has 8 bits
        b >>= 6;
        ret |= (b & 0x1FFFFF) << 8; // ret has 29 bits
        b >>= 22;
        ret |= (b & 0x1F) << 29; // ret has 34 bits
        b >>= 8;
        ret |= (b & 0x7) << 34; // ret has 37 bits

        return ret;
}


/****************** printBoard ********************
 Prints an msboard as a 7x7 grid of 1s and 0s 
    1 = marble
    0 = empty
Parameters: none
Returns: void
Notes:
    Outputs to stdout
***************************************************/
void msBoard::printBoard(std::ostream &stream) const 
{
    stream << "   " << 0 << " " << 1 << " " << 2 << " " << 3 << " " << 4 
              << " " << 5 << " " << 6 << '\n';
    for (int r = 0; r < NUM_ROWS; r++) {
        stream << r << "  ";
        if (r == 0 || r == 6) stream << "    ";
        if (r == 1 || r == 5) stream << "  ";
        for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
            stream << TO_STRING(getBit(board, r, c));
            if (c < COL_END_IDX[r]) stream << " ";
        }
        stream << "\n";
    }
}

/************ msBoard - private constructor *********
 Constructor to be used by private functions

Parameters: 
    Board b - the board that the new instance of the class will have
Returns: 
    An instance of msBoard class
Expects: 
    b should follow all the Board invariants
Notes:
    Will NOT CRE if b is structured poorly, but will exhibit undefined 
    behavior once other methods are called with this instance
*********************************/
msBoard::msBoard(Board b) 
{
    board = b;
}

/************ geCanonicalBoard *********
 Gets the canonical version of the given board - all posible symmetries and
    rotations are considered

Parameters: 
    Board b - the board that we analyze, passed by value
Returns: 
    a Board - canonical version of the given board
Expects: 
    b follows all Board invariants
Notes:
    Will return ill-formatted board if b is structured improperly
****************************************/
std::pair<msBoard, msBoard::Transform> msBoard::getCanonicalBits() const {
    Board best = board;
    Transform bestTransform = DEGREE_0;
    Board boards[NUM_ROTATIONS] = { board, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD, EMPTY_BOARD };

    // Yes, calling transformBoard is more modular, but performance is essential
    for (int i = 0; i < NUM_ROWS; i++) {
        Row row = getRow(board, i);
        Column col = getCol(board, i);

        boards[DEGREE_90]  |= Board(REVERSED[col]) << rowShift[i];
        boards[DEGREE_180] |= Board(REVERSED[row]) << rowShift[MAX_ROW - i];
        boards[DEGREE_270] |= Board(col)           << rowShift[MAX_ROW - i];
        boards[FLIP_H]     |= Board(REVERSED[row]) << rowShift[i];
        boards[FLIP_V]     |= Board(row)           << rowShift[MAX_ROW - i];
        boards[FLIP_DIAG]  |= Board(col)           << rowShift[i];
        boards[FLIP_ANTI]  |= Board(REVERSED[col]) << rowShift[MAX_ROW - i];
    }

    for (int i = 0; i < NUM_ROTATIONS; i++) {
        if (boards[i] < best) {
            best = boards[i];
            bestTransform = Transform(i);
        }
    }

    return { msBoard(best), bestTransform };
}


/************ msBoard::Move::toString *********
 Turns a Move into a string formatted as "row column direction"

Parameters: none
Returns: 
    a std::string containing the details of the Move
****************************************/
std::string msBoard::Move::toString() const
{
        // Extract destination row/col
        int destRow = -1, destCol = -1;
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
                if ((setBit >> bitIndex(r, c)) & 1ULL) {
                    destRow = r;
                    destCol = c;
                }
            }
        }

        // Extract origin and jumped bits
        int originRow = -1, originCol = -1;
        int jumpedRow = -1, jumpedCol = -1;

        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
                if ((clearBits >> bitIndex(r, c)) & 1ULL) {
                    // Decide which is origin vs jumped
                    if ((std::abs(r - destRow) == 2 && c == destCol) ||
                        (std::abs(c - destCol) == 2 && r == destRow)) {
                        originRow = r;
                        originCol = c;
                    } else {
                        jumpedRow = r;
                        jumpedCol = c;
                    }
                }
            }
        }
        (void) jumpedCol;
        (void) jumpedRow;

        // Determine direction
        std::string dir;
        if (originRow == destRow && originCol + 2 == destCol) dir = "right";
        else if (originRow == destRow && originCol - 2 == destCol) dir = "left";
        else if (originCol == destCol && originRow + 2 == destRow) dir = "down";
        else if (originCol == destCol && originRow - 2 == destRow) dir = "up";
        else dir = "unknown";

    return  std::to_string(originRow) + " " + std::to_string(originCol) +  " "
             + dir;

}


/************ isValidMove *********
 Checks to see whether a given move is valid on the current board state

Parameters: 
    int row   - the source row of the move 
    int col   - the source column of the move
    int toRow - the destination row of the move
    int toCol - the destination column of the move
Returns: 
    a bool - true if and only if there is a marble at (row, col) that can jump
             over a marble to get to (toRow, toCol), which is empty
Notes:
    Parameters can be unplayable indices - function will just return false
****************************************/
bool msBoard::isValidMove(int row, int col, int toRow, int toCol) const
{
    int rowDif = std::abs(row - toRow);
    int colDif = std::abs(col - toCol);


    if (toRow > MAX_ROW || toRow < 0 || toCol > MAX_COL || toCol < 0) {
        return false;
    }
    // source or destination is unplayable --> false
    if (!PLAYABLE[row][col] || !PLAYABLE[toRow][toCol]) {
        return false;
    }
    // move exactly 2 in one axis and 0 in the other
    if (!((rowDif == 2 && colDif == 0) || (rowDif == 0 && colDif == 2))) {
        return false;
    }
    // source must be a marble and destination must be empty
    if (!getBit(board, row, col) || getBit(board, toRow, toCol)) {
        return false;
    }

    // return if the middle position has a marble or not
    int midRow = (row + toRow) / 2;
    int midCol = (col + toCol) / 2;

    return getBit(board, midRow, midCol);
}
/************ getAMove *********
 Gets a Move struct corresponding to the given input

Parameters: 
    int row   - the source row of the move 
    int col   - the source column of the move
    int toRow - the destination row of the move
    int toCol - the destination column of the move
Returns: 
    a Move struct containing the set and clear bits to execute the move
Expects:
    Should only be called with parameters that correspond to a valid move
Notes:
    Will throw an error if the parameters are not valid
****************************************/
msBoard::Move msBoard::getAMove(int row, int col, int toRow, int toCol) const
{
    if (!isValidMove(row, col, toRow, toCol)) {
        throw std::runtime_error("Cannot get an invalid move\n");
    }

    int midRow = (row + toRow) / 2;
    int midCol = (col + toCol) / 2;

    Board set =   1ULL << bitIndex(toRow, toCol);
    Board clear = 1ULL << bitIndex(row, col);
    
    clear |=      1ULL << bitIndex(midRow, midCol);

    return msBoard::Move(set, clear);
}

/************ undoTransform *********
 Takes a Move and and a transform and undoes the effects of the transform on the
 Move

Parameters: 
    Move &m       - a reference to the Move to modify
    Transform t   - the transform we undo
Returns: void
Notes:
    May modify parameter m
****************************************/
void msBoard::undoTransform(Move &m, Transform t) const 
{
    if (t == DEGREE_0) return;

    Transform inv = inverseTransform(t);

    Board newSet   = transformBoard(m.setBit,    inv);
    Board newClear = transformBoard(m.clearBits, inv);

    m = Move(newSet, newClear);
}




/************ undoMove *********
 Undoes the inputted Move ont he board

Parameters: 
    const Move m - the move that we undo
Returns: 
    A new msBoard object with the given move undone
Expects:
    m should be a valid move to undo 
Notes:
    Does not error check
****************************************/
msBoard msBoard::undoMove(const Move m) const 
{
    Board next = (board & ~m.setBit) | m.clearBits;
    return msBoard(next);
}

/************ numRows *********
 Returns the number of rows on the board

Parameters: none
Returns: 
    a int containing the number of rows on the board
****************************************/
int msBoard::numRows() const {
    return NUM_ROWS;
}

/************ numCols *********
 Returns the number of columns on the board

Parameters: none
Returns: 
    a int containing the number of columns on the board
****************************************/
int msBoard::numCols() const {
    return NUM_COLS;
}