cmake_minimum_required(VERSION 3.10)
project(cs425-mp1-distributed-log-querier)

set(CMAKE_CXX_STANDARD 11)

add_executable(cs425-mp1-distributed-log-querier main.cpp client.cpp grep.cpp server.cpp server_func.h server_func.cpp client_func.cpp client_func.h)
#add_executable(cs425-mp1-distributed-log-querier grep.cpp server.cpp server_func.h server_func.cpp)
#add_executable(cs425-mp1-distributed-log-querier client.cpp client_func.h client_func.cpp)
target_link_libraries(cs425-mp1-distributed-log-querier pthread dl)


#add_executable(cs425-mp1-distributed-log-querier grep.cpp main.cpp)
