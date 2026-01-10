
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



#include <chrono>

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


#include "configuration.h"

#include <sys/mman.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <stack>
#include <sstream>

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

        std::vector<msBoard::Rotation> transforms;

        std::optional<msBoard::Move> incomingMove; // optional
    };

    /***************************  Enum and Constants: *************************/
    

    constexpr uint64_t BIT_COUNT  = 1ULL << 37;

    const int INIT_SEEN_SIZE = 8000000;
    
    const int INIT_MOVES_SIZE = 64;
    const int START_MOVE_IDX = 0;
    const int FIRST_MOVE_IDX = 0;

    /************************  Function declarations: *************************/
    std::vector<msBoard::Move> solvable(msBoard startBoard);
    std::vector<msBoard::Move> getMoveOrder(std::stack<StackFrame> dfs);
    /***************************** Functions: ********************************/
    

    struct IdentityHash {
        size_t operator()(uint64_t key) const {
            return key;
        }
    };

    /************ solvable *********
     checks whether a given board has a solution or not
    
    Parameters: 
        Board startBoard - a board to solve
    Returns: 
        A bool - true if solvable and false if not
    Expects: 
        board should be structured correctly for accurate results
    *********************************/

    int ct = 0;
    std::vector<msBoard::Move> solvable(msBoard startBoard) 
    {
        static msBitmap<msBoard, decltype(&msBoard::boardToBits)>* bits = nullptr;
        std::vector<msBoard::Rotation> transforms;
        std::vector<msBoard::Move> moves;
        void *seen;

        if (HAVE_16GB_RAM) {
            if (bits == nullptr) {
                bits = new msBitmap<msBoard, decltype(&msBoard::boardToBits)>
                                            (BIT_COUNT, &msBoard::boardToBits);
            }
            bits->clear();
            seen = bits;
        } else {
            auto tmpSet = new robin_hood::unordered_flat_set<uint64_t, IdentityHash>;
            tmpSet->reserve(INIT_SEEN_SIZE);
            seen = tmpSet;
            // auto tmpSet = new ankerl::unordered_dense::set<uint64_t>;
            tmpSet->reserve(INIT_SEEN_SIZE);
            seen = tmpSet;
        }

        moves.reserve(INIT_MOVES_SIZE);
        std::stack<StackFrame> dfs;

        auto [startCanonical, startTransform] = startBoard.getCanonicalBits();
        if (startTransform != msBoard::DEGREE_0) {
            transforms.push_back(startTransform);
        }
        startCanonical.validMoves(moves);
        dfs.emplace(StackFrame{ startCanonical, START_MOVE_IDX, moves.size(), 
                                FIRST_MOVE_IDX, transforms, 
                                std::nullopt });

        while (!dfs.empty()) {
            StackFrame &top = dfs.top();
            ct++;
            if (top.moveIndex >= top.moveEnd) {
                moves.reserve(top.movesStart);
                dfs.pop();
                continue;
            }

            const msBoard::Move m = moves[top.moveIndex++];
            msBoard nextBoard = top.board.applyMove(m);

            auto [canonical, transform] = nextBoard.getCanonicalBits();


            if (HAVE_16GB_RAM) {
                if (((msBitmap<msBoard, decltype(&msBoard::boardToBits)>*)seen)
                                        ->testAndSetBit(canonical)) {
                    continue;
                }
            } else {
                if (!(((robin_hood::unordered_flat_set<uint64_t, IdentityHash>*)seen)
                        ->insert(canonical.boardToBits()).second)) {
                    continue;
                }
            }

            if (nextBoard.hasWon()) {
                // cout << ((robin_hood::unordered_flat_set<uint64_t, IdentityHash>*)seen)->size() << endl;;
                if (!HAVE_16GB_RAM) 
                    delete (robin_hood::unordered_flat_set<uint64_t, IdentityHash>*)seen;
                // else delete (msBitmap<msBoard, decltype(&msBoard::boardToBits)> *)seen;
                return getMoveOrder(dfs);  
            }

            // Generate moves for the next step
            size_t start = moves.size();
            canonical.validMoves(moves);
            size_t end = moves.size();
            std::vector<msBoard::Rotation> newTrans = top.transforms;
            if (transform != msBoard::DEGREE_0) {
                newTrans.push_back(transform);
            }

            dfs.emplace(StackFrame{ canonical, start, end, start, newTrans, m });
        }

        return {};
    }


    /************ getMoveOrder *********
     Takes the stack from our dfs solvable algorithm and retrieves the move 
     order that solved the board
    
    Parameters: 
        std::stack<StackFrame> stack - contains the entire stack that we used
                                       during the solvable algorithm
    Returns: 
        A std::vector<msBoard::Move> that contains all the moves needed to solve
        the original board given to function solvable
    *********************************/
    std::vector<msBoard::Move> getMoveOrder(std::stack<StackFrame> stack) {
        std::vector<msBoard::Move> revSolution;
        std::vector<std::vector<msBoard::Rotation>> transforms;
        msBoard dummy;
        
        // Convert stack to vector so we can access previous elements
        std::vector<StackFrame> frames;
        while (!stack.empty()) {
            frames.push_back(stack.top());
            stack.pop();
        }
        // Collect moves with the PARENT's transforms
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].incomingMove) {
                revSolution.push_back(*frames[i].incomingMove);
                
                // Use the parent frame's transforms (i+1 since we popped in reverse)
                if (i + 1 < frames.size()) {
                    transforms.push_back(frames[i + 1].transforms);
                } else {
                    // First frame has no parent, use empty transforms
                    transforms.push_back(std::vector<msBoard::Rotation>());
                }
            }
        }
        
        for (size_t i = 0; i < revSolution.size(); i++) {
            msBoard::Move curr = revSolution[i];
            for (int j = transforms[i].size() - 1; j >= 0; j--) {  
                dummy.undoTransform(curr, transforms[i][j]);
            }
            revSolution[i] = curr;
        }
        
        // Reverse to get forward solution
        std::reverse(revSolution.begin(), revSolution.end());
        return revSolution;
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
    board = msBoard(msBoard::DEFAULT);
    // bits = new msBitmap<msBoard, decltype(&msBoard::boardToBits)>
    //                     (BIT_COUNT, &msBoard::boardToBits);
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
    // assert(board != nullptr);
    // delete board;
    // delete bits;
}


void msGame::playGame() 
{
    // board.printBoard();
    auto time1 = std::chrono::high_resolution_clock::now();
    std::vector<msBoard::Move> solution = solvable(board);
    // for (msBoard::Move m : solution) {
    //     m.print();
    // }
    auto time2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = time2 - time1;
    // cout << getBestMove() << endl;
    std::cout << "we went through " << ct << " stack frames in " << 
            elapsed.count() << " seconds" << std::endl;

    // std::cout << solvable(board) << std::endl;
}


/**************** getBestMove ***************
 Returns a std::string containing a move that leads to a solved board

 Parameters: none
 Returns: MoveInfo (string) structured as row col Direction
           e.g. "0 4 left"
          Will return an empty string if board is unsolvable
 Expects: nothing
 Notes: May take several seconds to run - must solve whole board
 ********************************************/
msGame::MoveInfo msGame::getBestMove() const
{
    std::vector<msBoard::Move> solution = solvable(board);

    if (solution.empty()) return "";
    return solution.front().toString();
}


/**************** getBoard ***************
 Puts the board into the provided ostream in the msBoard format

 Parameters: 
    std::ostream &stream - a reference to the output stream we put the board in
 Expects: nothing
 Notes: check msBoard class for how the board is outputted
 ********************************************/
void msGame::getBoard(std::ostream &stream) const
{
    board.printBoard(stream);
}

/**************** isValidMove ***************
 Checks to see if a given move is legal on the board

 Parameters: 
    unsigned row  - the row of the marble to be moved
    unsigned col  - the column of the marble to be moved
    Direction dir - the direction to move the selected marble in
 Returns: bool - true if and only if the move is possible on the current board
 Expects: row and col should be valid and correspond to a marble
          dir should make it a valid move
 Notes: Relies on msBoard's function to check move validity
 ********************************************/
bool msGame::isValidMove(unsigned row, unsigned col, Direction dir) const
{
    int destRow, destCol;

    destRow = (dir == UP)  ? (int)row - 2 : (dir == DOWN)  ? row + 2 : row;
    destCol = (dir == LEFT)? (int)col - 2 : (dir == RIGHT) ? col + 2 : col;

    return board.isValidMove(row, col, (unsigned) destRow, (unsigned) destCol);
}


/**************** makeMove ***************
 Executes a move on the board

 Parameters: 
    unsigned row  - the row of the marble to be moved
    unsigned col  - the column of the marble to be moved
    Direction dir - the direction to move the selected marble in
 Returns: bool - true if and only if the move was successfully executed
 Expects: row and col should be valid and correspond to a marble
          dir should make it a valid move
 Notes: Errors thrown from msBoard are caught
        This function only checks that the moves 
 ********************************************/
bool msGame::makeMove(unsigned row, unsigned col, Direction dir)
{
    if (!isValidMove(row, col, dir)) {
        return false;
    }
    
    int destRow = (dir == UP)   ? (int)row - 2 : (dir == DOWN)  ? row + 2 : row;
    int destCol = (dir == LEFT) ? (int)col - 2 : (dir == RIGHT) ? col + 2 : col;

    try { // extra safety check - should not need given isValidMoves()
        msBoard::Move move = board.getAMove(row, col, destRow, destCol);
        board = board.applyMove(move);
        moveHistory.push_back(move);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/**************** hasMoves ***************
 Checks to see if there are any valid moves to be made on the current board

 Parameters: none
 Returns: a bool - true if and only if there are legal moves on the board
 Expects: nothing
 ********************************************/
bool msGame::hasMoves() const
{
    std::vector<msBoard::Move> m;
    board.validMoves(m);

    return !m.empty();
}

/**************** hasWon ***************
 Returns whether the game is in a winning position (1 marble left) or not

 Parameters: none
 Returns: a bool - true if and only if the board is in winning position
 Expects: nothing
 ********************************************/
bool msGame::hasWon() const
{
    return board.hasWon();
}

/**************** undoMove ***************
 Undoes the last move made on the board

 Parameters: none
 Returns: a bool - true if and only if there was a move to undo
 Expects: nothing
 ********************************************/
bool msGame::undoMove() 
{
    if (moveHistory.empty()) {
        return false;
    }
    msBoard::Move lastMove = moveHistory.back();
    moveHistory.pop_back();
    board = board.undoMove(lastMove);
    return true;
}

/**************** getSolution ***************
 Returns the solution to the current board's state

 Parameters: none
 Returns: a string with each move formatted as row col direction, where row and
          col are 0-indexed and direction is either up, down, left, or right.
          Each move is separated by a newline in the string. It returns "No
          solution exists." if the board is unsolvable
 Expects: nothing
 Notes:  none
 ********************************************/
msGame::MoveInfo msGame::getSolution()
{
    std::vector<msBoard::Move> solution = solvable(board);
    std::stringstream moves;

    if (solution.empty()) return "No solution exists.";
    for (msBoard::Move m : solution) {
        moves << m.toString() << std::endl;
    }
    return moves.str();
}

void msGame::useCustomBoard(unsigned row, unsigned col) 
{
    board = msBoard(row, col);
    moveHistory.clear();
}