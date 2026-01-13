
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


#include "msSolver.h"
#include "msGame.h"
#include "msBoard.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>


/* 
This file is documented for programmers, not users
*/

/******************* Function definitions for msGame class: *******************/

/************ msGame *********
 Default constructor for the msGame class
 
Parameters: none
Returns: 
    An instance of the msGame class
Expects: Nothing
Notes:
    Initializes the game board to the DEFAULT value
*********************************/
msGame::msGame() 
{
    board = msBoard();
}

/************ ~msGame *********
 Destructor for the msGame class - nothing to delete
*********************************/
msGame::~msGame() {}



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
    std::vector<msBoard::Move> solution = msSolver::solve(board);

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

    destRow = (dir == UP)   ? (int)row - 2 : (dir == DOWN)  ? row + 2 : row;
    destCol = (dir == LEFT) ? (int)col - 2 : (dir == RIGHT) ? col + 2 : col;

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
    std::vector<msBoard::Move> solution = msSolver::solve(board);
    std::stringstream moves;

    if (solution.empty()) return "No solution exists.";
    for (msBoard::Move m : solution) {
        moves << m.toString() << std::endl;
    }
    return moves.str();
}

/**************** useCustomBoard ***************
 Allows users to pick which position of the board is empty at the start of the
 game

 Parameters: 
    unsigned row - the row of the position to be set to empty
    unsigned col - the row of the position to be set to empty
 Returns: void
 Expects: row and col should be valid indices
 Notes: Rather than throwing an error, the board will be set to its default
        state if invalid input is given
 ********************************************/
void msGame::useCustomBoard(unsigned row, unsigned col) 
{
    board = msBoard(row, col);
    moveHistory.clear();
}



// For testing purposes only 
void msGame::timeGame() 
{
    // pick a board to solve
    board = msBoard(1, 3);

    auto time1 = std::chrono::high_resolution_clock::now();
    std::vector<msBoard::Move> solution = msSolver::solve(board);
    auto time2 = std::chrono::high_resolution_clock::now();

    for (msBoard::Move m : solution) {
        std::cout << m.toString() << std::endl;
    }
    std::chrono::duration<double> elapsed = time2 - time1;

    std::cout << "solved in " << elapsed.count() << " seconds." << std::endl;
}