cmake_minimum_required(VERSION 3.10)
project(mp3-simple-distributed-file-system)

set(CMAKE_CXX_STANDARD 11)

add_executable(mp3-simple-distributed-file-system
        src/grep.cpp
        src/server.cpp
        src/server_func.cpp
        src/server_func.h
        src/server_membership.cpp
        src/server_membership.h
        src/client.cpp
        src/client_func.cpp
        src/client_func.h src/general.h src/general.cpp src/server_sdfs.cpp src/server_sdfs.h)
#add_executable(mp3-simple-distributed-file-system
#        src/grep.cpp
#        src/server.cpp
#        src/server_func.cpp
#        src/server_func.h
#        src/server_membership.cpp
#        src/server_membership.h
#        src/general.h src/general.cpp src/server_sdfs.cpp src/server_sdfs.h)
#add_executable(mp2-client client.cpp client_func.h client_func.cpp)
target_link_libraries(mp3-simple-distributed-file-system pthread dl)
