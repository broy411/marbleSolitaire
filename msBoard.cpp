/*
    msBoard.cpp
    January 4th, 2026
    Brendan Roy

    This file implements the msBoard.h interface. An msBoard is a board for
    French Marble Solitaire, where users can print the board, make moves, and
    generate valid moves. It's designed to be used by another class to
    implement Marble Solitaire.

*/

#include "msBoard.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <iostream>
#include "robin_hood.h"
#include <unordered_set>
#include <bitset>
#include <string>
#include <utility>
#include <vector>
#include <sys/mman.h>
#include <cstring>
#include <stdexcept>

#ifdef __BMI2__
#define HAVE_PEXT 1
#else
#define HAVE_PEXT 0
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



// Anonymous Namespace:
namespace {
    /*************************** Enum and Constants ***************************/
    /*
      Most boards have 8 equivalent states - one for each rotation / mirroring
    */
    enum Rotation {
        DEGREE_90 = 0, DEGREE_180, DEGREE_270,
        FLIP_H, FLIP_V, FLIP_DIAG, FLIP_ANTI
    };
    constexpr int NUM_ROTATIONS = 7;

    constexpr int NUM_ROWS = 7;
    constexpr int NUM_COLS = 7;
    constexpr int MAX_ROW = NUM_ROWS - 1;
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
    #define ROW_IDX(r) (MAX_BOARD_IDX - (r) * 7)
    constexpr int rowShift[NUM_ROWS] = {
                                        ROW_IDX(0) - (NUM_COLS - 1),
                                        ROW_IDX(1) - (NUM_COLS - 1),
                                        ROW_IDX(2) - (NUM_COLS - 1),
                                        ROW_IDX(3) - (NUM_COLS - 1),
                                        ROW_IDX(4) - (NUM_COLS - 1),
                                        ROW_IDX(5) - (NUM_COLS - 1),
                                        ROW_IDX(6) - (NUM_COLS - 1)
                                       };

    // const uint64_t DEFAULT_BOARD =    0x18FBFFFFEF8E0000; // - empty (0, 2) .14 fastest, .34rn
    const Board DEFAULT_BOARD =    0x38FBFFFEEF8E0000; // - empty (2,3) 63s -> 56 -> 43 (w O3) -> 25 (w 16GiB) -> 25 (w REVERSED - others got faster)
    // const uint64_t DEFAULT_BOARD =    0x38DBFFFFEF8E0000; // - empty (1, 3) 1.47s
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
        return MAX_BOARD_IDX - (r * NUM_COLS + c);
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
            masks[c] = (1ULL << (ROW_IDX(0) - c)) << 6 |
                       (1ULL << (ROW_IDX(1) - c)) << 5 |
                       (1ULL << (ROW_IDX(2) - c)) << 4 |
                       (1ULL << (ROW_IDX(3) - c)) << 3 |
                       (1ULL << (ROW_IDX(4) - c)) << 2 |
                       (1ULL << (ROW_IDX(5) - c)) << 1 |
                       (1ULL << (ROW_IDX(6) - c)) << 0;
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

/************ msBoard - public constructor *********
 Constructor for msBoard class - users can choose between initializing an
 EMPTY board (no marbles), DEFAULT board (empty spot starts in top left), or 
 choose a CUSTOM layout the user chooses

Parameters:
    BoardType b - which kind of board gets initialized
Returns: 
    An instance of the msBoard class
Notes:
    Allocates memory that is freed my destructor
*********************************/
msBoard::msBoard(BoardType bt) 
{

    if (bt == DEFAULT) {
        board = DEFAULT_BOARD;
    } else if (bt == EMPTY) {
        board = EMPTY_BOARD;
    } else {
        assert(1 == 0);
    }
}
msBoard::~msBoard() {}

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
void msBoard::validMoves(std::vector<const msBoard::Move*> &moves) const 
{

    for (const Move &m : ALL_MOVES) {
        if (((board & m.clearBits) == m.clearBits) &&
            ((board & m.setBit) == 0)) {
            moves.push_back(&m);
        }
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
    m is a valid move and is not a nullptr
Notes:
    Will CRE if m is nullptr
*********************************/
 msBoard msBoard::applyMove(const msBoard::Move *m) const 
 {
    assert(m != nullptr);

    Board next = (board | m->setBit) & ~(m->clearBits);
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
void msBoard::printBoard() const 
{
    for (int r = 0; r < NUM_ROWS; r++) {
        if (r == 0 || r == 6) std::cout << "  ";
        if (r == 1 || r == 5) std::cout << " ";
        for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
            std::cout << getBit(board, r, c);
        }
        std::cout << "\n";
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
msBoard msBoard::getCanonicalBoard() const
{
    // Identity rotation
    Board best = board;
    Board boards[NUM_ROTATIONS] = { EMPTY_BOARD, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD };
    for (int i = 0; i < NUM_ROWS; i++) {
        Row row = getRow(best, i);
        Column col = getCol(best, i);

        boards[DEGREE_90]  = insertRow(boards[DEGREE_90],  i, REVERSED[col]);
        boards[DEGREE_180] = insertRow(boards[DEGREE_180], MAX_ROW - i, 
                                                                REVERSED[row]);
        boards[DEGREE_270] = insertRow(boards[DEGREE_270], MAX_ROW - i, 
                                                                        col);
        boards[FLIP_H]     = insertRow(boards[FLIP_H],     i, REVERSED[row]);
        boards[FLIP_V]     = insertRow(boards[FLIP_V],     MAX_ROW - i, 
                                                                        row);
        boards[FLIP_DIAG]  = insertRow(boards[FLIP_DIAG],  i, col);
        boards[FLIP_ANTI]  = insertRow(boards[FLIP_ANTI],  MAX_ROW - i, 
                                                                REVERSED[col]);
    }


    for (Board curr : boards) {
        best = (curr < best) ? curr : best;
    }
    return msBoard(best);
}

void msBoard::Move::print() {
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

        std::cout << "(" << originRow << "," << originCol << ") -> ("
                << destRow << "," << destCol << ") " << dir << std::endl;
    }