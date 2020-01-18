cmake_minimum_required(VERSION 3.10)
project(mp2-distributed-group-membership)

set(CMAKE_CXX_STANDARD 11)

#add_executable(mp2-distributed-group-membership
#        src/grep.cpp
#        src/server.cpp
#        src/server_func.h
#        src/server_func.cpp
#        src/client.cpp
#        src/client_func.cpp
#        src/client_func.h
#        example/example_udp_client.cpp
#        example/example_udp_server.cpp)
add_executable(mp2-distributed-group-membership src/grep.cpp src/server.cpp src/server_func.h src/server_func.cpp)
#add_executable(mp2-client client.cpp client_func.h client_func.cpp)
target_link_libraries(mp2-distributed-group-membership pthread dl)


#add_executable(cs425-mp1-distributed-log-querier grep.cpp main.cpp)
