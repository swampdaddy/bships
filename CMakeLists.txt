cmake_minimum_required(VERSION 3.10)
project(battleship_contract CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(sysio.cdt)

add_contract(
   battleship
   battleship
   battleship.cpp
)
