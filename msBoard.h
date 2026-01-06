

#include <vector>

#ifndef MSBOARD_H_
#define MSBOARD_H_

class msBoard {
    using Board = uint64_t;

    public:
        struct Move;
        enum BoardType {EMPTY, DEFAULT, CUSTOM};

        msBoard(BoardType bt);
        ~msBoard();
        
        bool hasWon() const;
        void validMoves(std::vector<const Move*> &moves) const;
        msBoard applyMove(const Move *m) const;
        void printBoard() const;
        msBoard getCanonicalBoard() const;

        uint64_t boardToBits() const; 

        struct Move {
          public:
            void print();
          private:
            Board setBit;
            Board clearBits;
            Move(Board s, Board c) : setBit(s), clearBits(c) {}
            
            friend class msBoard;
        };

    private:
        msBoard(Board b);
        Board board;

        static std::vector<Move> setupAllMoves();
        static const std::vector<Move> ALL_MOVES;
};

#endif