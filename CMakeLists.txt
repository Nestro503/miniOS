cmake_minimum_required(VERSION 3.23)
project(miniOS C)

set(CMAKE_C_STANDARD 99)

add_executable(miniOS
        main.c
        src/process/process.c src/process/process.h
        src/scheduler/scheduler.c src/scheduler/scheduler.h
        src/io/io.c src/io/io.h
        src/sync/mutex.c src/sync/mutex.h
        src/sync/semaphore.c src/sync/semaphore.h src/trace/logger.c src/trace/logger.h src/trace/trace_event_types.h
        src/memory/memory.h src/memory/memory.c
)
