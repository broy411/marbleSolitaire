#include "msGame.h"
#include <iostream>
#include <sstream>


bool validInput(std::string input, msGame &game);
void clearScreen();


int main (int argc, char **argv) {
    (void) argc;
    (void) argv;
    clearScreen();
    msGame game = msGame();

    // game.playGame();
    // return 0;
    

    std::cout << "Hello and welcome to Marble Solitaire!\n\nThe goal of this" <<
                  " game is to leave only one marble on the board! You can " <<
                  "move marbles by jumping over another marble to an empty " << 
                  "spot! You may jump left, right, up, or down, but not " << 
                  "diagonally. " << "\n\nPlease enter your moves like this: "<< 
                  "\"row col direction\" where row and col are a digit 0 " <<
                  "through 6 that corresponds to a marble on the board, and " <<
                  "a direction is either \"left\", \"right\", \"up\", or " <<
                  "\"down\". The destination must not contain a marble. " <<
                  "Good luck!\n\n\n";
    unsigned moveNum     = 1;
    unsigned marblesLeft = 36;
    unsigned row;
    unsigned col;
    msGame::Direction dir;
    std::string input;
    std::string direction;
    // game.getBoard();


    std::cout << "enter the coordinates of the marble you'd like to start empty: ";
    std::cin >> row;
    std::cin >> col;
    game.useCustomBoard(row, col);




    game.getBoard(std::cout);
    std::cout << "\nPlease enter your move: \t\t\tMove " << moveNum << ", Marbles Left: " << marblesLeft << std::endl << std::endl;;
    while (game.hasMoves() && !std::cin.eof()) {
        std::getline(std::cin, input);
        if (validInput(input, game)) {
            std::stringstream in(input);
            in >> row;
            in >> col;
            in >> direction;
            dir = (direction == "right") ? msGame::RIGHT : 
                  (direction == "left")  ? msGame::LEFT  :
                  (direction == "up")    ? msGame::UP    : 
                                           msGame::DOWN;
            game.makeMove(row, col, dir);
        } else if (input == "hint") {
            std::cout << "\nBest move: " << game.getBestMove() << std::endl;
        } else if (input == "undo") {
            if (moveNum == 1) {
                std::cout << "No moves to undo!" << std::endl;
            } else {
                game.undoMove();
                moveNum -= 2;
                marblesLeft += 2;
            }
        } else if (input == "brendan is the coolest") {
            std::cout << "You're right! Clearly you're so intelligent you already know this is the solution:\n";
            std::cout << game.getSolution();  
        } else {
            std::cout << "Invalid move. Please enter again: ";
            continue;
        }
        std::cout << std::endl;
        game.getBoard(std::cout);
        std::cout << "\nPlease enter your move: \t\t\tMove " << ++moveNum << ", Marbles Left: " << --marblesLeft << std::endl << std::endl;;
    }
    if (game.hasWon()) {
        std::cout << "Woohoo! You win!" << std::endl;
    } else {
        std::cout << "oh no! You have lost!" << std::endl;
    }
    msGame game2;
    game2.playGame(); 
}

bool validInput(std::string input, msGame &game)
{
    std::stringstream in(input);

    int row, col;
    std::string direction;

    in >> row;
    in >> col;
    in >> direction;

    msGame::Direction dir = (direction == "right") ? msGame::RIGHT : 
                            (direction == "left")  ? msGame::LEFT  :
                            (direction == "up")    ? msGame::UP    : 
                                                     msGame::DOWN;

    return game.isValidMove(row, col, dir);
}

void clearScreen() 
{
    std::cout << "\033[2J\033[H" << std::flush;
}