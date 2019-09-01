cmake_minimum_required(VERSION 3.14)
project(ViaCLI)

set(CMAKE_CXX_STANDARD 17)

add_executable(ViaCLI main.cpp VEXSQL.cpp VEXSQL.hpp ViaAPI.cpp ViaAPI.hpp json.hpp)

target_link_libraries(ViaCLI curlpp curl sqlite3)