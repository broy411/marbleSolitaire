
/*
*     msSolver.h
*     By: Brendan Roy
*     Date: January 12th, 2025
*     Marble Solitaire
*
*     This file declares the msSolver.h methods - solve and isSolvable. The 
*     purpose of msSolver is to solve any given msBoard and return the solution.
*      
*/


#ifndef MSSOLVER_H_
#define MSSOLVER_H_

#include "msBoard.h"
#include <vector>

#include "configuration.h"

namespace msSolver {

    std::vector<msBoard::Move> solve(const msBoard& start);

    bool isSolvable(const msBoard& start);

    // #if HAVE_16GB_RAM
    // msBitmap<msBoard, decltype(&msBoard::boardToBits)> bitmap(BIT_COUNT, &msBoard::boardToBits);
    // #else 
    // robin_hood::unordered_flat_set<uint64_t> seen;
    // #endif
};

#endif
