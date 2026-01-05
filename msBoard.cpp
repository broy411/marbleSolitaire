#include "msBoard.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>

using Board = uint64_t;
using Row = uint8_t;
using Column = uint8_t;
#include <iostream>
#include "robin_hood.h"
#include <unordered_set>
#include <cstdint>
#include <assert.h>
#include <bitset>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <algorithm>

#include <sys/mman.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>

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

struct msBoard::Move {
    private:
    Board setBit;
    Board clearBits;

    Move(Board s, Board c) : setBit(s), clearBits(c) {}

    friend void printMove(const msBoard::Move *m);

    friend const std::vector<msBoard::Move> setupAllMoves();
    friend msBoard msBoard::applyMove(const Move*) const;
    friend void msBoard::validMoves(std::vector<const msBoard::Move*>&) 
                                                                        const;
};


// const std::vector<msBoard::Move> setupAllMoves();
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
    const Board DEFAULT_BOARD =    0x38FBFFFEEF8E0000; // - empty (2,3) 63s -> 56 -> 43 (w O3) -> 25 (w 16GiB)
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
    
    /**************************** Private Functions ***************************/


    // last constant to declare
    // const std::vector<MoveInternal> ALL_MOVES = setupAllMoves();


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
        return ((b >> (ROW_IDX(0) - c)) & 1ULL) << 6 |
            ((b >> (ROW_IDX(1) - c)) & 1ULL) << 5 |
            ((b >> (ROW_IDX(2) - c)) & 1ULL) << 4 |
            ((b >> (ROW_IDX(3) - c)) & 1ULL) << 3 |
            ((b >> (ROW_IDX(4) - c)) & 1ULL) << 2 |
            ((b >> (ROW_IDX(5) - c)) & 1ULL) << 1 |
            ((b >> (ROW_IDX(6) - c)) & 1ULL) << 0;
    }


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




    inline Board getBit(Board b, int r, int c) {
        return (b >> bitIndex(r, c)) & 1ULL;
    }

    struct MoveInternal {
        Board setBit;
        Board clearBits;
        MoveInternal(Board s, Board c) { setBit = s; clearBits = c; }
    };

    std::vector<MoveInternal> setupAllMovesInternal();
    const std::vector<MoveInternal> ALL_MOVES = setupAllMovesInternal();

 std::vector<MoveInternal> setupAllMovesInternal()  {
    std::vector<MoveInternal> moves;

    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
            if (!PLAYABLE[r][c]) continue;

            unsigned src = bitIndex(r, c);

            if (r >= 2 && PLAYABLE[r-1][c] && PLAYABLE[r-2][c])
                moves.emplace_back(
                    Board(1) << bitIndex(r-2, c),
                    (Board(1) << src) | (Board(1) << bitIndex(r-1, c)));

            if (r <= 4 && PLAYABLE[r+1][c] && PLAYABLE[r+2][c])
                moves.emplace_back(
                    Board(1) << bitIndex(r+2, c),
                    (Board(1) << src) | (Board(1) << bitIndex(r+1, c)));

            if (c >= 2 && PLAYABLE[r][c-1] && PLAYABLE[r][c-2])
                moves.emplace_back(
                    Board(1) << bitIndex(r, c-2),
                    (Board(1) << src) | (Board(1) << bitIndex(r, c-1)));

            if (c <= 4 && PLAYABLE[r][c+1] && PLAYABLE[r][c+2])
                moves.emplace_back(
                    Board(1) << bitIndex(r, c+2),
                    (Board(1) << src) | (Board(1) << bitIndex(r, c+1)));
            }
        }
        return moves;
    }
}
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
    // const std::vector<msBoard::Move> setupAllMoves()  {
    //     std::vector<msBoard::Move> moves;

    //     for (int r = 0; r < NUM_ROWS; r++) {
    //         for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
    //             if (!PLAYABLE[r][c]) continue;

    //             unsigned src = bitIndex(r, c);

    //             if (r >= 2 && PLAYABLE[r-1][c] && PLAYABLE[r-2][c])
    //                 moves.push_back(msBoard::Move(
    //                     Board(1) << bitIndex(r-2, c),
    //                     (Board(1) << src) | (Board(1) << bitIndex(r-1, c)))
    //                 );

    //             if (r <= 4 && PLAYABLE[r+1][c] && PLAYABLE[r+2][c])
    //                 moves.push_back(msBoard::Move(
    //                     Board(1) << bitIndex(r+2, c),
    //                     (Board(1) << src) | (Board(1) << bitIndex(r+1, c)))
    //                 );

    //             if (c >= 2 && PLAYABLE[r][c-1] && PLAYABLE[r][c-2])
    //                 moves.push_back(msBoard::Move(
    //                     Board(1) << bitIndex(r, c-2),
    //                     (Board(1) << src) | (Board(1) << bitIndex(r, c-1)))
    //                 );

    //             if (c <= 4 && PLAYABLE[r][c+1] && PLAYABLE[r][c+2])
    //                 moves.push_back(msBoard::Move(
    //                     Board(1) << bitIndex(r, c+2),
    //                     (Board(1) << src) | (Board(1) << bitIndex(r, c+1)))
    //                 );
    //         }
    //     }
    //     return moves;
    // }



/* ======================= msBoard API ========================== */
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
    // for (const Move &m : ALL_MOVES) {
    //     if (((board & m.clearBits) == m.clearBits) &&
    //         ((board & m.setBit) == 0)) {
    //         moves.push_back(&m);
    //     }
    // }
    for (const MoveInternal& m : ALL_MOVES) {
        if (((board & m.clearBits) == m.clearBits) &&
            ((board & m.setBit) == 0)) {
            moves.push_back(reinterpret_cast<const Move*>(&m));
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
 msBoard msBoard::applyMove(const msBoard::Move *m) const {
    assert(m != nullptr);

    Board next = (board | m->setBit) & ~m->clearBits;
    return msBoard(next);
}
/************ boardToBits *********
Converts a board to an index in the 'seen' bitmap

Parameters: 
    Board startBoard - a board to solve
Returns: 
    A bool - true if solvable and false if not
Expects: 
    board should follow all Board invariants
Notes:
    board
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
void msBoard::printBoard() const 
{
    for (int r = 0; r < NUM_ROWS; r++) {
        if (r == 0 || r == 6) std::cout << "  ";
        if (r == 1 || r == 5) std::cout << " ";
        for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++)
            std::cout << getBit(board, r, c);
        std::cout << "\n";
    }
}
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
    // printBoard();
    // std::cout << "best is : " << std::bitset<49>(board) << std::endl;
    Board boards[NUM_ROTATIONS] = { EMPTY_BOARD, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD, EMPTY_BOARD, EMPTY_BOARD, 
                                    EMPTY_BOARD };


    for (int i = 0; i < NUM_ROWS; i++) {
        Row row = getRow(best, i);
        Column col = getCol(best, i);
        Row reversedRow = reverse7(row);
        Column reversedCol = reverse7(col);

        boards[DEGREE_90]  = insertRow(boards[DEGREE_90],  i, reversedCol);
        boards[DEGREE_180] = insertRow(boards[DEGREE_180], MAX_ROW - i, 
                                                                reversedRow);
        boards[DEGREE_270] = insertRow(boards[DEGREE_270], MAX_ROW - i, 
                                                                        col);
        boards[FLIP_H]     = insertRow(boards[FLIP_H],     i, reversedRow);
        boards[FLIP_V]     = insertRow(boards[FLIP_V],     MAX_ROW - i, 
                                                                        row);
        boards[FLIP_DIAG]  = insertRow(boards[FLIP_DIAG],  i, col);
        boards[FLIP_ANTI]  = insertRow(boards[FLIP_ANTI],  MAX_ROW - i, 
                                                                reversedCol);
    }

    for (Board curr : boards) {
        best = (curr < best) ? curr : best;
    }
    return msBoard(best);
}

uint64_t msBoard::getRawBoard() {
    return board;
}

void printMove(const msBoard::Move *m) {
        // Extract destination row/col
        int destRow = -1, destCol = -1;
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = COL_START_IDX[r]; c <= COL_END_IDX[r]; c++) {
                if ((m->setBit >> bitIndex(r, c)) & 1ULL) {
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
                if ((m->clearBits >> bitIndex(r, c)) & 1ULL) {
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