cmake_minimum_required(VERSION 3.16)
project(multithreading_utils)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(multithreading_utils Semaphore.h WaitingThreadsQueue.h)
add_subdirectory(test)

set_target_properties(multithreading_utils PROPERTIES LINKER_LANGUAGE CXX)