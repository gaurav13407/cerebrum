CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -I src

cerebrum: src/main.cpp src/board.cpp src/board.h
	$(CXX) $(CXXFLAGS) src/main.cpp src/board.cpp -o cerebrum

clean:
	rm -f cerebrum *.o
