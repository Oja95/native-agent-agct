cmake_minimum_required(VERSION 3.8)
project(native_agent_agct)

set(CMAKE_CXX_STANDARD 11)

include_directories(
        /usr/lib/jvm/java-8-oracle/include/
        /usr/lib/jvm/java-8-oracle/include/linux
)

set(SOURCE_FILES stacktraces.h main.h main.cpp)
add_library(native_agent_agct SHARED ${SOURCE_FILES})