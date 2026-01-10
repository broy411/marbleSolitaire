
#include <cstdint>
#include <vector>

#ifndef MSBOARD_H_
#define MSBOARD_H_

#include <string>

class msBoard {

    using Board = uint64_t;

    public:
        struct Move;
        /*
        Most boards have 8 equivalent states - one for each rotation / mirroring
        */
        enum Rotation { DEGREE_0 = 0, DEGREE_90, DEGREE_180, DEGREE_270,
                        FLIP_H, FLIP_V, FLIP_DIAG, FLIP_ANTI
        };
        
        enum BoardType {EMPTY, DEFAULT, CUSTOM};

        msBoard();
        msBoard(unsigned row, unsigned col);
        msBoard(BoardType bt);
        ~msBoard();


        
        bool hasWon() const;
        void validMoves(std::vector<Move> &moves) const;
        msBoard applyMove(const Move m) const;
        void printBoard(std::ostream &stream) const;
        std::pair<msBoard, msBoard::Rotation> getCanonicalBits() const;
        msBoard getCanonicalBoard() const;
        Move getAMove(int row, int col, int toRow, int toCol) const;
        bool isValidMove(int row, int col, int toRow, int toCol) const;

        void undoTransform(Move &m, Rotation rot) const;


        uint64_t boardToBits() const; 

        msBoard undoMove(const Move m) const;

        int numRows() const;
        int numCols() const;

        struct Move {
          public:
            void print() const;
            std::string toString() const;
            Move(const Move&) = default;
            Move &operator=(const Move &other) {
                if (this == &other) return *this;
                this->setBit = other.setBit;
                this->clearBits = other.clearBits;
                return *this;
            }
            

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
