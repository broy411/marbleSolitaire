/*
    main.cpp
    January 8th, 2026
    Brendan Roy

    This is the playable version of Marble Solitaire - a user can pick their
    starting empty position and then play the game by entering moves in the
    terminal. Input is taken in through stdin and all output goes to stdout.
    Please be patient when requesting hints because sometimes it takes a minute.

*/

#include "msGame.h"

#include <iostream>
#include <sstream>

const int INIT_MARBLE_CT = 36;
const int INIT_MOVE_NUM  = 1;

bool validInput(const std::string &input, msGame &game);
void clearScreen();
msGame setupGame();
void playGame(msGame &game);


/************ main *********
 Performs the functionality of the program as a whole - starts and prompts the 
 user to play Marble Solitaire

Parameters: 
    int argc     - the number of arguments in the command line
    char *argv[] - a pointer to an array of characters holding the actual 
                   things typed in the command line
Returns: 
    an int - should return 0 (EXIT_SUCCESS)
Expects: 
    argc should be 1 (just the program name)
Notes:
    Controls all I/O
****************************************/
int main (int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    clearScreen();

    msGame game = setupGame();

    playGame(game);
    
    if (game.hasWon()) std::cout << "Woohoo! You win!"      << std::endl;
    else               std::cout << "Oh no! You have lost!" << std::endl;

    std::cout << "\n\nThanks for playing!\n";

    return 0;
}

/************ playGame *********
 Controls move input and execution while interacting with the user

Parameters: 
    msGame &game - a reference to an msGame object that we play
Returns: void
Expects: 
    game should not have any moves executed yet
Notes:
    Prompts the user until the game is completed - could infinite loop if
    continuous invalid input is entered
****************************************/
void playGame(msGame &game)
{
    unsigned moveNum     = INIT_MOVE_NUM;
    unsigned marblesLeft = INIT_MARBLE_CT;
    unsigned row;
    unsigned col;
    msGame::Direction dir;
    std::string input;
    std::string direction;

    game.getBoard(std::cout);
    std::cout << "\nPlease enter your move: \t\t\tMove " << moveNum << 
                 ", Marbles Left: " << marblesLeft << std::endl << std::endl;;
    while (game.hasMoves() && std::getline(std::cin, input)) {
        clearScreen();

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
            marblesLeft--;
        } else if (input == "hint") {
            std::cout << "\nBest move: " 
                      << ((game.getBestMove() == "") 
                            ? "No solution for this board. Try undoing!" 
                            : (game.getBestMove())) << std::endl;
        } else if (input == "undo") {
            if (marblesLeft == INIT_MARBLE_CT) {
                std::cout << "No moves to undo!" << std::endl;
            } else {
                game.undoMove();
                marblesLeft++;
            }
        } else if (input == "brendan is the coolest") { // :)
            std::cout << "You're right! Clearly you're so intelligent you" << 
                         " already know this is the solution:\n";
            std::cout << game.getSolution();  
        } else {
            std::cout << "Invalid move. Please enter again: \n";
            game.getBoard(std::cout);
            continue;
        }
        game.getBoard(std::cout);
        std::cout << "\nPlease enter your move: \t\t\tMove " << ++moveNum << 
                    ", Marbles Left: " << marblesLeft << std::endl << std::endl;
    }
}

/************ setupGame *********
 Gets all information needed to initialize an msGame object

Parameters: none
Returns: an msGame object with either custom or default start board
Expects: 
    game should not have any moves executed yet
Notes:
    Prints to stdout and reads in from stdin to set up the game
****************************************/
msGame setupGame()
{
    msGame game = msGame();

    int row, col;
    std::string dummy;

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

    std::cout << "enter the coordinates of the marble you'd like to remove: ";
    std::cin >> row;
    std::cin >> col;
    game.useCustomBoard(row, col);

    std::getline(std::cin, dummy); //read in the newline

    return game;
}

/************ validInput *********
 Determines whether or not a user's input is valid - input is valid iff the user
 provides the coordinates of a marble and a direction for that marble to jump
 and the move is executable on the board

Parameters: 
    std::string input - the line of input from the user (e.g. "0 4 left")
    msGame &game      - a reference to the msGame that we play

Returns: 
    a bool - true if and only if the input if valid for the given game
Expects: nothing
Notes:
    parameters can be wrong - function is desigmed to handle improper input
****************************************/
bool validInput(const std::string &input, msGame &game)
{
    std::stringstream in(input);

    int row, col;
    std::string direction;

    if (!(in >> row >> col >> direction)) {
        return false;
    }
    msGame::Direction dir;
    if (direction == "right") {
        dir = msGame::RIGHT;
    } else if (direction == "left") {
        dir = msGame::LEFT;
    } else if (direction == "up") {
        dir = msGame::UP;
    } else if (direction == "down") {
        dir = msGame::DOWN;
    } else {
        return false;
    }

    return game.isValidMove(row, col, dir);
}

/************ clearScreen *********
 Clear the terminal screen - next output goes to the top of the screen

Parameters: none
Returns: void
Expects: nothing
Notes:
    Only works on systems with ANSI
    Not essential to game function - if no ANSI, just remove this function
****************************************/
void clearScreen() 
{
    std::cout << "\033[2J\033[H" << std::flush;
}