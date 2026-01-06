#########################################################
#           PROJECT MARBLE SOLITAIRE Makefile           #
# By: Brendan Roy		        	        #
# Date: December 29, 2024				#
#########################################################

CXX      = clang++
CXXFLAGS = -g -O3 -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -mbmi2

# This rule builds msGame executable
# The $^ refers to all the object files listed as dependencies
# The $@ means the name of the executable will be the name of the target
msGame: main.o msGame.o msBoard.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# This rule builds main.o
main.o: main.cpp msGame.h 
	$(CXX) $(CXXFLAGS) -c main.cpp

# This rule builds msGame.o
msGame.o: msGame.cpp msGame.h msBoard.h
	$(CXX) $(CXXFLAGS) -c msGame.cpp

msBoard.o: msBoard.cpp msBoard.h
	$(CXX) $(CXXFLAGS) -c msBoard.cpp

# remove executables, object code, and temporary files from the current folder
clean: 
	rm *.o *~ a.out
