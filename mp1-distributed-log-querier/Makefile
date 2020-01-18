CXX = g++
CXXFLAGS = -g -Wall -std=c++11
CXXFLAGS_THREAD = -pthread -ldl

all: server client

server: server.cpp server_func.cpp grep.cpp
	$(CXX) $(CXXFLAGS) server.cpp server_func.cpp grep.cpp $(CXXFLAGS_THREAD) -o server

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp client_func.cpp $(CXXFLAGS_THREAD) -o client

clean:
	rm client server

.PHONY: clean
