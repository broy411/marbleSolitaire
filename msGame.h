#include <string>
#include <utility>
#include <vector>
#include <array>
#include <cstdint>
#include "msBoard.h"
#include "msBitmap.h"


#ifndef MSGAME_H
#define MSGAME_H

class msGame {

    
    public:

        enum Direction { UP, DOWN, LEFT, RIGHT };
        // structured as "(row, col) -> up | down | left | right"
        using MoveInfo = std::string; 

        msGame();
        ~msGame();
        
        void playGame();

        void getBoard(std::ostream &stream) const;
        MoveInfo getBestMove() const ;
        MoveInfo getSolution();
        bool isValidMove(unsigned row, unsigned col, Direction dir) const;
        bool makeMove(unsigned row, unsigned col, Direction dir);
        bool undoMove();
        bool hasMoves() const;
        bool hasWon() const;
        void useCustomBoard(unsigned row, unsigned col);

        

    private:
        msBoard board;

        std::vector<msBoard::Move> moveHistory;
};

#endif
