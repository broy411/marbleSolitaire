#include <string>
#include <utility>
#include <vector>

using namespace std;

class msSolver {
    const static int NUM_COLS = 7;
    public:
        bool canBeSolved(int board[][NUM_COLS]);
        string validNextMove(int board[][NUM_COLS]);
    private:
        struct Position {
            int row;
            int col;
        };
        bool solvable()
        enum Direction {UP, RIGHT, DOWN, LEFT};
        vector<pair<Position, Direction>> validMoves;
    
};