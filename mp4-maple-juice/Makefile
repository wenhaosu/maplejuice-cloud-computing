CXX = g++
CXXFLAGS = -g -Wall -std=c++11
CXXFLAGS_THREAD = -pthread -ldl

all: server client

server: src/server.cpp src/server_func.cpp src/grep.cpp src/server_membership.cpp src/general.cpp src/server_sdfs.cpp src/server_maplejuice.cpp
	$(CXX) $(CXXFLAGS) src/server.cpp src/server_func.cpp src/grep.cpp src/server_membership.cpp src/general.cpp src/server_sdfs.cpp src/server_maplejuice.cpp $(CXXFLAGS_THREAD) -o server

client: src/client.cpp src/general.cpp
	$(CXX) $(CXXFLAGS) src/client.cpp src/client_func.cpp src/general.cpp $(CXXFLAGS_THREAD) -o client

clean:
	rm client server

.PHONY: clean
