cmake_minimum_required(VERSION 3.23)
project(miniOS C)

set(CMAKE_C_STANDARD 99)

add_executable(miniOS
        main.c
        src/process/process.c src/process/process.h
        src/scheduler/scheduler.c src/scheduler/scheduler.h
        src/io/io.c src/io/io.h
        src/trace/trace.c src/trace/trace.h
        src/sync/sync.c src/sync/sync.h
        )
