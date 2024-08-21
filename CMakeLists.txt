cmake_minimum_required(VERSION 3.1)
project(OS2)

set(CMAKE_C_STANDARD 11)
include_directories(.)

add_executable(OS2 uthreads.h uthreads.cpp Scheduler.cpp Scheduler.h Thread.cpp Thread.h)