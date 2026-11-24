CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
all: heritage
heritage: src/main.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) -Isrc src/main.cpp -o heritage
clean:
	rm -f heritage
