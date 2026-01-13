/*
*     msSolver.cpp
*     By: Brendan Roy
*     Date: January 12th, 2025
*     Marble Solitaire
*
*     This file defines the msSolver.h methods - solve and isSolvable. It uses
*     a performance-optimized algorithm to sovle the board as fast as possible.
*     
  Possible future performance optimizations:
    - Find a way to determine a 35 bit encoding from a canonical board with
      37 bits. There are >2^34 possible canonical boards and this would reduce
      memory
    - Rather than checking all possible moves, only check ones in the area
      affected by the previous move
    - Utilize a heuristic strategy to sort the moves in order from most likely
       to lead to a solution to least likely
    - Any improvement to getCanonicalBoard (reducing # of times called or 
      speeding up the function itself) would be most impactful. This function
      accounts for 55% of total runtime 
*      
*/

#include "msSolver.h"
#include "msBoard.h"
#include <stack>
#include <optional>
#include <utility>
#include <sys/mman.h>
#include "msBitmap.h"
#include "robin_hood.h"

namespace {

    /************************** structs and types: ****************************/

    /************ StackFrame *********
     The StackFrame struct contains all information about which moves are yet to 
        be played on the board during our DFS 'solve' algorithm

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

        std::vector<msBoard::Transform> transforms;

        std::optional<msBoard::Move> incomingMove; 
    };

    /************ IdentityHash *********
     This struct serves as an identity hash function - just retunrs the uint64_t
        it was given. Used for the unordered set
    *********************************/
    struct IdentityHash {
        size_t operator()(uint64_t key) const {
            return key;
        }
    };

    /******************************* Constants: *******************************/
    constexpr uint64_t BIT_COUNT  = 1ULL << 37;



    const int INIT_MOVES_SIZE = 64;
    const int START_MOVE_IDX = 0;
    const int FIRST_MOVE_IDX = 0;


    /************************* Function declarations: *************************/
    std::vector<msBoard::Move> runDFS( 
                    std::stack<StackFrame> dfs,
                    msBitmap<msBoard, decltype(&msBoard::boardToBits)>& seen,
                    std::vector<msBoard::Move> moves);

    std::vector<msBoard::Move> getMoveOrder(std::stack<StackFrame> dfs);


    /******************************* Functions: *******************************/
    
    /************ runDFS *********
     Performs the dfs search algorithm and finds a solution to the given
     board state
    
    Parameters: 
        std::stack<StackFrame> dfs:
            - dfs is the stack we use to keep track of board states
        msBitmap<msBoard, &msBoard::boardToBits> seen:
            - bitmap holds all the 'seen' boards so we don't revisit them
        std::vector<msBoard::Move> moves
            - moves holds all moves to try throughout the solution search
    Returns: 
        std::vector<msBoard::Move> - a vector of Moves that hold the valid 
                                     solution to the original board state - 
                                     all transformation considerations should be
                                     ignored - they're removed by return-time
    Expects: 
        dfs should hold the initial StackFrame
        bitmap / set should be cleared
        moves should hold the initial valid moves of the board
    Notes: Will return incorrect results if not called correctly - should really
           only be used by the solve function
    *********************************/
    std::vector<msBoard::Move> runDFS(
                    std::stack<StackFrame> dfs,
                    msBitmap<msBoard, decltype(&msBoard::boardToBits)>& seen,
                    std::vector<msBoard::Move> moves)
    {
        while (!dfs.empty()) {
            StackFrame &top = dfs.top();
            if (top.moveIndex >= top.moveEnd) {
                moves.reserve(top.movesStart);
                dfs.pop();
                continue;
            }
            const msBoard::Move m = moves[top.moveIndex++];
            msBoard nextBoard = top.board.applyMove(m);

            auto [canonical, transform] = nextBoard.getCanonicalBits();

            if (seen.testAndSet(canonical)) continue;

            // Generate moves for the next step
            size_t start = moves.size();
            canonical.validMoves(moves);
            size_t end = moves.size();
            std::vector<msBoard::Transform> newTrans = top.transforms;
            if (transform != msBoard::DEGREE_0) {
                newTrans.push_back(transform);
            }

            if (nextBoard.hasWon()) {
                dfs.emplace(StackFrame{ canonical, start, end, start, newTrans, 
                                        m });
                return getMoveOrder(dfs);  
            }
            dfs.emplace(StackFrame{ canonical, start, end, start, newTrans, m });
        }
        return {};
    }

    /************ getMoveOrder *********
     Takes the stack from our dfs solve algorithm and retrieves the move 
     order that solved the board
    
    Parameters: 
        std::stack<StackFrame> stack - contains the entire stack that we used
                                       during the solve algorithm
    Returns: 
        A std::vector<msBoard::Move> that contains all the moves needed to solve
        the original board given to function solve
    *********************************/
    std::vector<msBoard::Move> getMoveOrder(std::stack<StackFrame> stack) {
        std::vector<msBoard::Move> revSolution;
        std::vector<std::vector<msBoard::Transform>> transforms;
        msBoard dummy;
        
        // Convert stack to vector so we can access previous elements
        std::vector<StackFrame> frames;
        while (!stack.empty()) {
            frames.push_back(stack.top());
            stack.pop();
        }
        // get transforms
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].incomingMove) {
                revSolution.push_back(*frames[i].incomingMove);
                
                // Use the parent frame's transforms
                if (i + 1 < frames.size()) {
                    transforms.push_back(frames[i + 1].transforms);
                } else {
                    transforms.push_back(std::vector<msBoard::Transform>());
                }
            }
        }
        // undo transforms
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

/************ solve *********
 Takes in a board and solves it

Parameters: 
    const Board &startBoard - a constant reference to the board to solve
Returns: 
    A std::vector<msBoard::Move> that contains all the moves needed to solve
    the original board given to function solve
Notes: 
    Will return an empty vector if the board is unsolvable
*********************************/
std::vector<msBoard::Move> msSolver::solve(const msBoard& startBoard)
{
    static msBitmap<msBoard, decltype(&msBoard::boardToBits)> 
                                        seen(BIT_COUNT, &msBoard::boardToBits);

    std::vector<msBoard::Transform> transforms;
    std::vector<msBoard::Move> moves;

    seen.clear();

    moves.reserve(INIT_MOVES_SIZE);
    std::stack<StackFrame> dfs;

    // get initial canonical board and transform - start algorithm
    auto [startCanonical, startTransform] = startBoard.getCanonicalBits();

    if (startTransform != msBoard::DEGREE_0) {
        transforms.push_back(startTransform);
    }
    startCanonical.validMoves(moves);
    dfs.emplace(StackFrame{ startCanonical, START_MOVE_IDX, moves.size(), 
                            FIRST_MOVE_IDX, transforms, 
                            std::nullopt });

    return runDFS(dfs, seen, moves);
}



bool msSolver::isSolvable(const msBoard& start)
{
    return !solve(start).empty();
}