#########################################################
#           PROJECT MARBLE SOLITAIRE Makefile           #
# By: Brendan Roy		        	        #
# Date: December 29, 2024				#
#########################################################


CXX      = clang++
CXXFLAGS = -g -O3 -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -MMD -MP


msGame: main.o msGame.o msBoard.o msSolver.o
	$(CXX) $(CXXFLAGS) $^ -o $@

main.o: main.cpp msGame.h msSolver.h
	$(CXX) $(CXXFLAGS) -c main.cpp

msGame.o: msGame.cpp msGame.h msBoard.h 
	$(CXX) $(CXXFLAGS) -c msGame.cpp

msSolver.o: msSolver.cpp msSolver.h msBoard.h msBitmap.h
	$(CXX) $(CXXFLAGS) -c msSolver.cpp

msBoard.o: msBoard.cpp msBoard.h
	$(CXX) $(CXXFLAGS) -c msBoard.cpp

clean:
	rm -f *.o *.d *~ a.out msGame

.PHONY: clean

-include *.d
