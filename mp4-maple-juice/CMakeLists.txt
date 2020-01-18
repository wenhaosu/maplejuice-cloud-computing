cmake_minimum_required(VERSION 3.10)
project(mp4-maple-juice)

set(CMAKE_CXX_STANDARD 11)

file(GLOB srcfiles
        "src/*.h"
        "src/*.cpp"
        "maplejuice/*.cpp"
        )

add_executable(mp4-maple-juice ${srcfiles})
target_link_libraries(mp4-maple-juice pthread dl)
