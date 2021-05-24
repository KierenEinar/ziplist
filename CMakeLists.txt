cmake_minimum_required(VERSION 3.16)
project(ziplist C)

set(CMAKE_C_STANDARD 11)

add_executable(ziplist main.c ziplist.c ziplist.h)