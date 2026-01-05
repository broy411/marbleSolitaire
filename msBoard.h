

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
        // void printMove(const Move *m);
        uint64_t getRawBoard();

        uint64_t boardToBits() const; 

    private:
        msBoard(Board b);
        Board board;
};

#endif