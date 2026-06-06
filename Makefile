CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -I src

cerebrum: src/main.cpp src/board.cpp src/movegen.cpp src/board.h src/movegen.h
	$(CXX) $(CXXFLAGS) src/main.cpp src/board.cpp src/movegen.cpp -o cerebrum

clean:
	rm -f cerebrum *.o
