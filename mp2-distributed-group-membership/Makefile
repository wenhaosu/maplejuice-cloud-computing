CXX = g++
CXXFLAGS = -g -Wall -std=c++11
CXXFLAGS_THREAD = -pthread -ldl

all: server client

server: src/server.cpp src/server_func.cpp src/grep.cpp
	$(CXX) $(CXXFLAGS) src/server.cpp src/server_func.cpp src/grep.cpp $(CXXFLAGS_THREAD) -o server

client: src/client.cpp
	$(CXX) $(CXXFLAGS) src/client.cpp src/client_func.cpp $(CXXFLAGS_THREAD) -o client

clean:
	rm client server

.PHONY: clean
