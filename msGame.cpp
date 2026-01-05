
/*
*     msGame.cpp
*     By: Brendan Roy
*     Date: December 30th, 2025
*     Marble Solitaire
*
*     This file implements the msGame class - a user can play Marble (or Peg)
*     Solitaire on an ASCII-printed board regularly or can ask for help from the
*     game itself. A user can see the next correct move or can be shown the
*     solution to their current board at any time (if one exists). 
*/


#include "msGame.h"
#include "msBoard.h"
#include "msBitmap.h"
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


/* 
This file is documented for programmers, not users

Possible future performance optimizations:
    - Find a way to determine a 35 bit encoding from a canonical board with
      37 bits. There are >2^34 possible canonical boards and this would reduce
      memory
    - Rather than checking all possible moves, only check ones in the area
      affected by the previous move
    - Utilize a more efficient heuristic model to sort the moves
    - Any improvement to getCanonicalBoard (reducing # of times called or 
      speeding up the function itself) would be most impactful. This function
      accounts for 55% of total runtime 
*/

namespace {

    /************************** structs and types: ****************************/
    

    /************ StackFrame *********
     The StackFrame struct contains all information about which moves are yet to 
     be played on the board during our DFS 'solvable' algorithm
    
    Members: 
        Board board - the board that we are playing on
        size_t moveIndex  - the index of the current move we are executing
        size_t moveEnd    - the index of the last valid move on our board
        size_t movesStart - the index of the first valid move on our board

        All the indices mentioned above correspond to a shared buffer of moves
    *********************************/
    struct StackFrame {
        msBoard board;
        size_t moveIndex;
        size_t moveEnd;
        size_t movesStart;  

        const msBoard::Move *incomingMove; 
    };

    /***************************  Enum and Constants: *************************/
    

    constexpr uint64_t BIT_COUNT  = 1ULL << 37;

    const int INIT_SEEN_SIZE = 3000000;
    
    const int INIT_MOVES_SIZE = 64;
    const int START_MOVE_IDX = 0;
    const int FIRST_MOVE_IDX = 0;

    /************************  Function declarations: *************************/
    bool solvable(msBoard startBoard);

    /***************************** Functions: ********************************/
    

    /************ solvable *********
     checks whether a given board has a solution or not
    
    Parameters: 
        Board startBoard - a board to solve
    Returns: 
        A bool - true if solvable and false if not
    Expects: 
        board should be structured correctly for accurate results
    *********************************/
    bool solvable(msBoard startBoard) 
    {

        // Shared move buffer (pointers into ALL_MOVES)
        std::vector<const msBoard::Move*> moves;
        void *seen;

        if (HAVE_16GB_RAM) {
            msBitmap<msBoard, decltype(&msBoard::boardToBits)>* tmp =
                new msBitmap<msBoard, decltype(&msBoard::boardToBits)>
                            (BIT_COUNT, &msBoard::boardToBits);
            seen = tmp; // seen = an msBitmap
        } else {
            seen = new robin_hood::unordered_flat_set<uint64_t>;
            ((robin_hood::unordered_flat_set<uint64_t> *)seen)->
                reserve(INIT_SEEN_SIZE);
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        moves.reserve(INIT_MOVES_SIZE);

        std::stack<StackFrame> dfs;

        startBoard = startBoard.getCanonicalBoard();

        startBoard.printBoard();

        startBoard.validMoves(moves); // initial possible move
        
        dfs.push({ startBoard, START_MOVE_IDX, moves.size(), FIRST_MOVE_IDX, 
                   nullptr });

        while (!dfs.empty()) {
            StackFrame &top = dfs.top();

            if (top.moveIndex >= top.moveEnd) {
                moves.resize(top.movesStart);  
                dfs.pop();
                continue;
            }

            const msBoard::Move *m = moves[top.moveIndex++];
            msBoard nextBoard = top.board.applyMove(m);

            // if (__builtin_popcountll(nextBoard) >= 8) // less than 15 moves from root 
            nextBoard = nextBoard.getCanonicalBoard();

            if (HAVE_16GB_RAM) {
                if (((msBitmap<msBoard, decltype(&msBoard::boardToBits)>*) seen)->
                        testAndSetBit(nextBoard)) {
                    continue;
                }
            } else { 
                if (!(((robin_hood::unordered_flat_set<uint64_t> *)seen)->
                        insert(nextBoard.boardToBits()).second)) {
                    continue;
                }
            }
            
            if (nextBoard.hasWon()) {
                std::vector<const msBoard::Move*> solution;

                // getMoveOrder();
                std::stack<StackFrame> tmp = dfs;
                while (!tmp.empty()) {
                    const StackFrame& f = tmp.top();
                    if (f.incomingMove)
                        solution.push_back(f.incomingMove);
                    tmp.pop();
                }

                std::reverse(solution.begin(), solution.end());

                // solution now contains the exact move sequence
                // for (const msBoard::Move* mv : solution) {
                //     msBoard::printMove(mv);
                // }
                auto t1 = std::chrono::high_resolution_clock::now();
                std::cout << std::chrono::duration<double>(t1 - t0).count() << "\n";

                return true;
            }

            // Generate moves for next step
            size_t start = moves.size();
            nextBoard.validMoves(moves);
            size_t end = moves.size(); // now has more moves

            dfs.push({ nextBoard, start, end, start, m });
        }
        return false;
    }

}

/************ msGame *********
 Constructor for the msGame class
 
Parameters: none
Returns: 
    An instance of the msGame class
Expects: Nothing
Notes:
    Initializes the game board to the DEFAULT_BOARD value - CRE if out of memory
    Allocates memory in form of a new Impl -> must be freed by destructor
*********************************/
msGame::msGame() 
{
    board = new msBoard(msBoard::DEFAULT);
}

/************ ~msGame *********
 Destructor for the msGame class

Parameters: None
Returns: void
Expects: 
    The impl1 class variable has not been deleted yet
Notes:
    Deallocates the impl1 variable
    Will CRE if impl1 is nullptr
*********************************/
msGame::~msGame() 
{
    assert(board != nullptr);
    delete board;
}

void msGame::playGame() 
{
    board->printBoard();

    int *b = new int;
    (void) b;
    
    std::cout << solvable(*board) << std::endl;
}
