cmake_minimum_required(VERSION 3.14)
project(mpi_projekt C)

set(CMAKE_C_STANDARD 99)

add_executable(mpi_projekt main.c)

include_directories(/usr/local/include/)