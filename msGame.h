
/*
*     msGame.h
*     By: Brendan Roy
*     Date: December 30th, 2025
*     Marble Solitaire
*
*     This file declares the msGame class - a user can play French Marble 
*     Solitaire on a custom or default starting board. Each game starts with a 
*     single empty position on the board (the rest have marbles), and each move 
*     consists of a marble 'jumping' over another marble to an empty spot. The
*     goal of the game is to only have 1 marble left on the board
*/


#include "msBoard.h"
#include "msBitmap.h"

#include <string>
#include <vector>
#include <cstdint>


#ifndef MSGAME_H
#define MSGAME_H

class msGame {
    public:
        enum Direction { UP, DOWN, LEFT, RIGHT };
        
        // structured as "row col [up | down | left | right]"
        using MoveInfo = std::string; 

        msGame();
        ~msGame();
        
        void getBoard(std::ostream &stream) const;
        MoveInfo getBestMove() const ;
        MoveInfo getSolution();
        bool isValidMove(unsigned row, unsigned col, Direction dir) const;
        bool makeMove(unsigned row, unsigned col, Direction dir);
        bool undoMove();
        bool hasMoves() const;
        bool hasWon() const;
        void useCustomBoard(unsigned row, unsigned col);

        // for testing:
        void timeGame();
    private:
        msBoard board;
        std::vector<msBoard::Move> moveHistory;
};

#endif
