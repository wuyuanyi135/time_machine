cmake_minimum_required(VERSION 3.23)
project(time_machine)

set(CMAKE_CXX_STANDARD 20)

add_executable(time_machine main.cpp)
add_executable(buzzer buzzer.cpp)
add_executable(custom_state_all_callbacks custom_state_all_callbacks.cpp)
