#include <string>
#include <utility>
#include <vector>
#include <array>
#include "msBoard.h"

using namespace std;

#ifndef MSGAME_H
#define MSGAME_H
class msGame {
    /* 
       If your computer has 16GiB of RAM available to use, set this to 1. The 
       typical speedup with it enabled is about 1.75x, but set it 0 if space is 
       unavailable. That said, there is a small overhead cost of using the
       larger bitmap of ~0.5s
    */
    #define HAVE_16GB_RAM 1
    
    public:
        msGame();
        ~msGame();
        
        void playGame();

        string getBestMove();
        string getSolution();
        void makeMove();
        void useCustomBoard();
        void useDefaultBoard();


    private:
        msBoard *board;
};

#endif