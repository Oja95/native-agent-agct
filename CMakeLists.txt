cmake_minimum_required(VERSION 3.8)
project(native_agent_agct)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES library.cpp library.h)
add_library(native_agent_agct SHARED ${SOURCE_FILES})